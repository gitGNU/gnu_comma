//===-- driver/SourceManager.cpp ------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "SourceManager.h"
#include "comma/parser/DepParser.h"

#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/SetVector.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

using namespace comma::driver;

SourceManager::SourceManager(IdentifierPool &IdPool,
                             Diagnostic &Diag)
    : IdPool(IdPool),
      Diag(Diag)
{
    // Get the contents of the current working directory.
    //
    // FIXME: Eventually we will want to process a full set of directories as
    // given by command line options and compile-time paths.
    typedef std::set<llvm::sys::Path> PathSet;
    llvm::sys::Path cwd = llvm::sys::Path::GetCurrentDirectory();
    PathSet listing;
    std::string msg;

    if (cwd.getDirectoryContents(listing, &msg)) {
        llvm::errs() << "Directory lookup failed: " << msg;
        abort();
    }

    // Process each ".cms" file in the directory and establish the canonical
    // (lowercase) basename for each entry.  Populate our PathTable with the
    // results.
    for (PathSet::iterator I = listing.begin(); I != listing.end(); ++I) {
        if (I->getSuffix() == "cms") {
            std::string key = I->getBasename();
            std::transform(key.begin(), key.end(), key.begin(), tolower);
            PathTable.insert(PathTable.begin(), PathMap::value_type(key, *I));
        }
    }
}

SourceItem *SourceManager::getOrCreateSourceItem(llvm::sys::Path &path) {
    const std::string &key = path.str();
    SourceMap::iterator I = SourceTable.find(key);
    if (I != SourceTable.end())
        return I->second;
    else {
        SourceItem *SD = new SourceItem(path);
        SourceTable.insert(SourceTable.begin(),
                           SourceMap::value_type(key, SD));
        return SD;
    }
}

bool SourceManager::loadDependencies(SourceItem *Item)
{
    // If this item already has dependents associated with it we must not
    // process this item again.
    if (Item->numDependents())
        return true;

    DepParser::DepSet dependents;
    TextProvider provider(0, Item->getSourcePath());
    DepParser parser(provider, IdPool, Diag);

    if (!parser.parseDependencies(dependents))
        return false;

    // Locate each dependency of Item in the path table.
    for (DepParser::DepSet::iterator I = dependents.begin();
         I != dependents.end(); ++I) {
        Location loc = I->first;
        std::string target = I->second;
        std::transform(target.begin(), target.end(), target.begin(), tolower);
        PathMap::iterator result = PathTable.find(target);

        if (result == PathTable.end()) {
            SourceLocation sloc = provider.getSourceLocation(loc);
            Diag.report(sloc, diag::UNIT_RESOLUTION_FAILED) << target;
            return false;
        }

        SourceItem *SubItem = getOrCreateSourceItem((*result).second);
        Item->addDependency(*SubItem);
    }

    return true;
}

bool SourceManager::loadSubDependencies(SourceItem *Item)
{
    // Process each dependency needed by the given SourceItem.
    for (SourceItem::iterator I = Item->begin(); I != Item->end(); ++I) {
        SourceItem *SubItem = *I;
        CycleEntry handle(CycleTable, SubItem);

        if (!handle) {
            reportCycleDependencies(SubItem);
            return false;
        }

        if (!(loadDependencies(SubItem) && loadSubDependencies(SubItem)))
            return false;

        CycleTable.remove(SubItem);
    }
    return true;
}

bool SourceManager::processDependencies(SourceItem *Item)
{
    CycleEntry handle(CycleTable, Item);
    if (!(loadDependencies(Item) && loadSubDependencies(Item)))
        return false;
    return true;
}

SourceItem *SourceManager::getSourceItem(llvm::sys::Path &path)
{
    SourceItem *Item = getOrCreateSourceItem(path);
    if (Item->numDependents() == 0) {
        if (!processDependencies(Item))
            return 0;
    }
    return Item;
}

void SourceManager::reportCycleDependencies(SourceItem *Item)
{
    // Find the given item in the CycleTable.
    typedef CycleSet::iterator iterator;

    iterator E = CycleTable.end();
    iterator I = std::find(CycleTable.begin(), E, Item);
    assert(I != E && "SourceItem missing from cycle table!");

    llvm::errs() << "Circular dependency detected:\n";
    while (I != E) {
        SourceItem *A = *I;
        SourceItem *B;
        ++I;

        B = (I == E) ? Item : *I;

        llvm::errs() << "  `" << A->getSourcePath().getLast()
                     << "' depends on `"
                     << B->getSourcePath().getLast() << "'\n";
    }
}

namespace {

void insertDependencies(SourceItem *Item, llvm::SetVector<SourceItem*> &sources)
{
    SourceItem::iterator I = Item->begin();
    SourceItem::iterator E = Item->end();

    for ( ; I != E; ++I) {
        SourceItem *Dep = *I;
        if (sources.count(Dep))
            sources.remove(Dep);
        sources.insert(Dep);
        insertDependencies(Dep, sources);
    }
}

} // end anonymous namespace.

void SourceItem::extractDependencies(std::vector<SourceItem*> &dependencies)
{
    typedef llvm::SetVector<SourceItem*> SourceSet;

    SourceSet sources;
    sources.insert(this);
    insertDependencies(this, sources);
    dependencies.insert(dependencies.end(), sources.begin(), sources.end());
}
