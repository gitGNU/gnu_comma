//===-- basic/TextManager.cpp --------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/basic/TextManager.h"

#include "llvm/Support/raw_ostream.h"

#include <cstdlib>

using namespace comma;

TextManager::~TextManager()
{
    typedef ProviderMap::iterator iterator;
    for (iterator I = providers.begin(); I != providers.end(); ++I)
        delete I->second;
}

TextProvider &TextManager::create(const llvm::sys::Path& path)
{
    if (nextProviderID > Location::MAX_LOCATION_STAMP) {
        llvm::errs() << "Too many files managed (location stamp overflow).";
        abort();
    }

    assert(providers.count(nextProviderID) == 0 && "Corrupt TextManager table!");

    TextProvider *provider = new TextProvider(nextProviderID, path);
    providers[nextProviderID] = provider;
    nextProviderID++;
    return *provider;
}

SourceLocation TextManager::getSourceLocation(const Location loc) const
{
    unsigned stamp = loc.getStamp();
    TextProvider *provider = providers.lookup(stamp);

    assert(provider && "Location does not map to any TextProvider.");
    return provider->getSourceLocation(loc);
}
