//===-- ast/SubroutineRef.h ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief This file defines the SubroutineRef class.
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_SUBROUTINE_REF_HDR_GUARD
#define COMMA_AST_SUBROUTINE_REF_HDR_GUARD

#include "comma/ast/Decl.h"

namespace comma {

/// This class represents a reference to a subroutine name.  It provides
/// location information as to where the the reference occurs in source code.
/// It also provides a set of declaration nodes representing a overloaded family
/// of declarations.
///
/// Note that this class is a miscellaneous member of the AST in that it does
/// not belong to one of a major branched (Type, Decl, Expr).
class SubroutineRef : public Ast {

    // The type used to represent the collection referenced declarations.
    //
    // We use the smallest SmallVector we can since all SubroutineRefs are
    // eventually resolved to a single decl (when the program is well formed).
    typedef llvm::SmallVector<SubroutineDecl*, 1> DeclVector;

public:
    /// Creates a SubroutineRef representing the given declaration.
    SubroutineRef(Location loc, SubroutineDecl *decl)
        : Ast(AST_SubroutineRef), loc(loc) {
        decls.push_back(decl);
    }

    /// Creates a SubroutineRef given a set of SubroutineDecl's.
    ///
    /// All subroutine decls provided must be either ProcedureDecl's or
    /// FunctionDecl's and be identically named.
    SubroutineRef(Location loc, SubroutineDecl **decls, unsigned numDecls)
        : Ast(AST_SubroutineRef),
          decls(decls, decls + numDecls), loc(loc) {
        verify();
    }

    /// Creates a SubroutineRef given an iterator pair over a set of
    /// SubroutineDecl's.
    ///
    /// All subroutine decls provided must be either ProcedureDecl's or
    /// FunctionDecl's and be identically named.
    template <class I>
    SubroutineRef(Location loc, I begin, I end)
        : Ast(AST_SubroutineRef),
          decls(begin, end), loc(loc) {
        verify();
    }

    /// Returns the IdentifierInfo common to all of the referenced declarations.
    IdentifierInfo *getIdInfo() const { return decls[0]->getIdInfo(); }

    /// Returns a C-string representing the name common to all overloads.
    const char *getString() const { return getIdInfo()->getString(); }

    /// Returns the location of this reference.
    Location getLocation() const { return loc; }

    /// Extends this reference with another declaration.
    void addDeclaration(SubroutineDecl *srDecl);

    /// Returns true if this references more than one declaration.
    bool isOverloaded() const { return numDeclarations() > 1; }

    /// Returns true if this is a reference to a set of function declarations.
    bool referencesFunctions() const {
        return llvm::isa<FunctionDecl>(getDeclaration(0));
    }

    /// Returns true if this is a reference to a set of procedure declarations.
    bool referencesProcedures() const {
        return llvm::isa<ProcedureDecl>(getDeclaration(0));
    }

    /// Returns the number of declarations associated with this reference.
    unsigned numDeclarations() const { return decls.size(); }

    //@{
    /// Returns the i'th delaration associated with this reference.
    const SubroutineDecl *getDeclaration(unsigned i) const {
        assert(i < numDeclarations() && "Index out of range!");
        return decls[i];
    }

    SubroutineDecl *getDeclaration(unsigned i) {
        assert(i < numDeclarations() && "Index out of range!");
        return decls[i];
    }
    //@}

    /// Returns true if this reference contains the given subroutine
    /// declaration.
    bool contains(const SubroutineDecl *srDecl) const;

    /// Returns true if this reference contains a subroutine declaration with
    /// the given type.
    bool contains(const SubroutineType *srType) const;

    /// Resolves this reference to point at the given declaration.
    ///
    /// This method removes all declarations from the reference, then adds in
    /// the given declaration.  This method is used once overloads have been
    /// resolved.
    void resolve(SubroutineDecl *srDecl) {
        decls.clear();
        decls.push_back(srDecl);
    }

    //@{
    /// Iterators over the referenced declarations.
    typedef DeclVector::iterator iterator;
    iterator begin() { return decls.begin(); }
    iterator end() { return decls.end(); }

    typedef DeclVector::const_iterator const_iterator;
    const_iterator begin() const { return decls.begin(); }
    const_iterator end() const { return decls.end(); }
    //@}

private:
    /// Forward iterator over the set of declarations parameterized over the
    /// expected type of the declarations.
    template <class T>
    class SubroutineDeclIter {

        mutable SubroutineRef *ref;
        mutable unsigned index;

        /// Creates an iterator over the connectives of the given SubroutineRef.
        SubroutineDeclIter(SubroutineRef *ref)
            : ref(ref),
              index(0) { }

        friend class SubroutineRef;

    public:
        /// Creates a sentinal iterator.
        SubroutineDeclIter() :
            ref(0),
            index(0) { }

        SubroutineDeclIter(const SubroutineDeclIter &iter)
            : ref(iter.ref),
              index(iter.index) { }

        T *operator *() {
            assert(ref && "Cannot dereference an empty iterator!");
            return llvm::cast<T>(ref->getDeclaration(index));
        }

        const T *operator *() const {
            assert(ref && "Cannot dereference an empty iterator!");
            return llvm::cast<T>(ref->getDeclaration(index));
        }

        bool operator ==(const SubroutineDeclIter &iter) const {
            return (this->ref == iter.ref && this->index == iter.index);
        }

        bool operator !=(const SubroutineDeclIter &iter) const {
            return !this->operator==(iter);
        }

        SubroutineDeclIter &operator ++() {
            if (++index == ref->numDeclarations()) {
                ref = 0;
                index = 0;
            }
            return *this;
        }

        const SubroutineDeclIter &operator ++() const {
            if (++index == ref->numDeclarations()) {
                ref = 0;
                index = 0;
            }
            return *this;
        }

        SubroutineDeclIter operator ++(int) const {
            SubroutineDeclIter tmp = *this;
            this->operator++();
            return tmp;
        }
    };

public:
    //@{
    /// Specialized iterators to traverse the set of functions or procedures.
    /// Note that any given SubroutineRef only holds functions or procedures and
    /// never a mix of the two.  The iterators will always be empty if the
    /// iterator type does not match the kind of subroutines this reference
    /// contains.

    typedef SubroutineDeclIter<FunctionDecl> fun_iterator;
    fun_iterator begin_functions() {
        if (this->referencesFunctions())
            return fun_iterator(this);
        else
            return fun_iterator();
    }
    fun_iterator end_functions() { return fun_iterator(); }

    typedef const SubroutineDeclIter<FunctionDecl> const_fun_iterator;
    const_fun_iterator begin_functions() const {
        if (this->referencesFunctions()) {
            SubroutineRef *ref = const_cast<SubroutineRef*>(this);
            return const_fun_iterator(ref);
        }
        else
            return const_fun_iterator();
    }
    const_fun_iterator end_functions() const { return const_fun_iterator(); }

    typedef SubroutineDeclIter<ProcedureDecl> proc_iterator;
    proc_iterator begin_procedures() {
        if (this->referencesProcedures())
            return proc_iterator(this);
        else
            return proc_iterator();
    }
    proc_iterator end_procedures() { return proc_iterator(); }

    typedef const SubroutineDeclIter<ProcedureDecl> const_proc_iterator;
    const_proc_iterator begin_procedures() const {
        if (this->referencesProcedures()) {
            SubroutineRef *ref = const_cast<SubroutineRef*>(this);
            return const_proc_iterator(ref);
        }
        else
            return const_proc_iterator();
    }
    const_proc_iterator end_procedures() const { return const_proc_iterator(); }
    //@}

    static bool classof(const SubroutineRef *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_SubroutineRef;
    }

private:
    DeclVector decls;
    Location loc;

    /// Used to check that the current set of declarations are consitent.
    void verify();

    /// Used to check that the current set of declarations all have the given
    /// name. If \p isaFunction is true, also checks that all declarations are
    /// functions, otherwise that all declarations are procedures.
    void verify(IdentifierInfo *idInfo, bool isaFunction);
};

} // end comma namespace.

#endif
