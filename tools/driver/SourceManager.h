//===-- driver/SourceManager.h -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_DRIVER_SOURCEMANAGER_HDR_GUARD
#define COMMA_DRIVER_SOURCEMANAGER_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/basic/Diagnostic.h"
#include "comma/basic/TextProvider.h"
#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/System/Path.h"
#include <map>

namespace comma {
namespace driver {

using namespace comma;

class SourceItem;

class SourceManager {

public:
    SourceManager(IdentifierPool &IdPool, Diagnostic &Diag);

    /// Returns a SourceItem object for the given path.
    ///
    /// If \p path has already been associated with a SourceItem object the
    /// original object is returned.  Otherwise a new SourceItem is created.
    /// Ownership remains the the SourceManager.
    ///
    /// If an error is encountered during processing this method will
    /// emit a diagnostic and return 0.
    SourceItem *getSourceItem(llvm::sys::Path &path);

    /// Returns the number of SourceItem objects that are currently managed.
    unsigned numSources() const { return SourceTable.size(); }

private:
    IdentifierPool &IdPool;
    Diagnostic &Diag;

    /// Map from canonical basename's to a corresponding path object.
    typedef std::map<std::string, llvm::sys::Path> PathMap;
    PathMap PathTable;

    /// Map from pathnames to the corresponding SourceItem object.
    typedef std::map<std::string, SourceItem*> SourceMap;
    SourceMap SourceTable;

    /// An ordered set of SourceItems (effectively a stack) used to detect
    /// circular dependencies.
    typedef llvm::SetVector<SourceItem*> CycleSet;
    CycleSet CycleTable;

    /// Simple RAII structure which populates the given set with a source item
    /// and ensures the items removal on destruction.  A CycleEntry can be
    /// evaluated as a boolean value yielding true if the item was added to the
    /// set and false otherwise.
    class CycleEntry {
    public:
        CycleEntry(CycleSet &Set, SourceItem *Item)
            : Set(Set), Item(Item) {
            Inserted = Set.insert(Item);
        }

        ~CycleEntry() {
            if (Inserted)
                Set.remove(Item);
        }

        operator bool() const { return Inserted; }

        bool Inserted;
        CycleSet &Set;
        SourceItem *Item;

    private:
        CycleEntry(const CycleEntry &handle);              // Do not implement.
        CycleEntry &operator = (const CycleEntry &handle); // Likewise.
    };

    /// Returns an existing or creates a new SourceItem object.
    SourceItem *getOrCreateSourceItem(llvm::sys::Path &path);

    /// Populates the given SourceItem with its immediate sub-dependencies.
    bool loadDependencies(SourceItem *Item);

    /// Processes the dependencies of the given SourceItem's dependents.
    bool loadSubDependencies(SourceItem *Item);

    /// Processes the dependencies and sub dependencies of the given SourceItem.
    bool processDependencies(SourceItem *Item);

    /// Diagnoses a dependence cycle starting with the given SourceItem.
    void reportCycleDependencies(SourceItem *Item);
};


/// \class
/// \brief Encapsulates the dependency
class SourceItem {

public:
    /// Return the path which backs this source.
    ///
    /// Every SourceItem object has a source file associated with it.
    const llvm::sys::Path &getSourcePath() const { return sourcePath; }

    /// Return the path to the compiled llvm IR file for this SourceItem.
    ///
    /// If an IR file has not been associated with this SourceItem,
    /// llvm::sys::Path::empty() returns true.
    const llvm::sys::Path &getIRPath() const { return IRPath; }

    /// Return the path to the compiled llvm bitcode file for this SourceItem.
    ///
    /// If a bitcode file has not been associated with this SourceItem,
    /// llvm::sys::Path::empty() returns true.
    const llvm::sys::Path &getBitcodePath() const { return bitcodePath; }

    /// Sets the path pointing to the IR file corresponding to this SourceItem.
    void setIRPath(const llvm::sys::Path &path) { IRPath = path; }

    /// Sets the path pointing to the bitcode file corresponding to this
    /// SourceItem.
    void setBitcodePath(const llvm::sys::Path &path) { bitcodePath = path; }

    /// Associate a dependency with this item.
    void addDependency(SourceItem &dep) {
        // Taking address of dep is fine since all SourceDependencies are
        // managed by a single SourceManager.
        dependents.push_back(&dep);
    }

    /// Populates the given vector with a preorder traversal of the dependency
    /// graph rooted at this SourceItem.
    void extractDependencies(std::vector<SourceItem*> &dependencies);

    //@{
    /// Iterators over this source files set of dependencies.
    typedef std::vector<SourceItem*>::iterator iterator;
    iterator begin() { return dependents.begin(); }
    iterator end() { return dependents.end(); }

    typedef std::vector<SourceItem*>::const_iterator const_iterator;
    const_iterator begin() const { return dependents.begin(); }
    const_iterator end() const { return dependents.end(); }
    //@}

    /// Returns the number of dependents associated with this source file.
    unsigned numDependents() const { return dependents.size(); }

    /// Associates a compilation unit with this source item.
    void setCompilation(CompilationUnit *cunit) {
        this->cunit = cunit;
    }

    /// Returns true if this source item has a compilation unit associated with
    /// it.
    bool hasCompilation() const { return cunit != 0; }

    //@{
    /// Returns the compilation unit associated with this source unit, or null
    /// if there is no associated compilation unit.
    CompilationUnit *getCompilation() { return cunit; }
    const CompilationUnit *getCompilation() const { return cunit; }
    //@}

private:
    friend class SourceManager;

    /// Constructor for use by SourceManager.
    SourceItem(const llvm::sys::Path &pathname)
        : sourcePath(pathname),
          cunit(0) { }

    /// Only allow destruction thru SourceManager.
    ~SourceItem() { }

    SourceItem(const SourceItem &SD); // Do not implement.
    SourceItem &operator=(const SourceItem &SD); // Likewise.

    llvm::sys::Path sourcePath;
    llvm::sys::Path IRPath;
    llvm::sys::Path bitcodePath;
    CompilationUnit *cunit;
    std::vector<SourceItem*> dependents;
};


bool processDependencies(llvm::sys::Path &path,
                         IdentifierPool &idPool, Diagnostic &diag);

} // end namespace driver
} // end namespace comma

namespace llvm {

/// Specialize llvm::GraphTraits to provide iterators over the dependencies for
/// a given SourceItem.
template <>
struct GraphTraits<comma::driver::SourceItem> {

    typedef comma::driver::SourceItem NodeType;
    typedef comma::driver::SourceItem::iterator ChildIteratorType;

    static NodeType *getEntryNode(const comma::driver::SourceItem &Item) {
        return const_cast<NodeType*>(&Item);
    }
    static ChildIteratorType child_begin(NodeType *Item) {
        return Item->begin();
    }
    static ChildIteratorType child_end(NodeType *Item) {
        return Item->end();
    }
};

} // end llvm namespace.


#endif
