//===-- basic/TextManager.h ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_BASIC_TEXTMANAGER_HDR_GUARD
#define COMMA_BASIC_TEXTMANAGER_HDR_GUARD

#include "comma/basic/TextProvider.h"
#include "llvm/ADT/DenseMap.h"

namespace comma {

/// \class TextManager
/// \brief Organizes a collection of TextProvider instances.
class TextManager {

public:
    TextManager() : nextProviderID(0) { }

    /// Destroys a TextManager and all associated TextProvider instances.
    ~TextManager();

    /// \brief Creates a TextProvider over the given file.
    ///
    /// Initializes a TextProvider to manage the contents of the given file.
    /// The provided path must name a readable text file, or if the path
    /// specifies a file name "-", then read from all of stdin instead.  If the
    /// path is invalid, this method will simply call abort.
    ///
    /// \param path The file used to back the TextProvider.
    TextProvider &create(const llvm::sys::Path& path);


    /// \brief Returns a SourceLocation object corresponding to the given
    /// Location object.
    SourceLocation getSourceLocation(const Location loc) const;

private:
    /// The next available TextProvider identifier.
    unsigned nextProviderID;

    /// Map from provider ID's to managed TextProvider objects.
    typedef llvm::DenseMap<unsigned, TextProvider*> ProviderMap;
    ProviderMap providers;
};

} // end comma namespace.

#endif
