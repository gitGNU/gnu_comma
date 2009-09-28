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
/// location information as to where the reference occurs in source code.  It
/// also provides a set of declaration nodes representing a overloaded family of
/// declarations.
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
    IdentifierInfo *getIdInfo() const {
        assert(!empty() && "Empty SubroutineRef!");
        return decls[0]->getIdInfo();
    }

    /// Returns a C-string representing the name common to all overloads.
    const char *getString() const { return getIdInfo()->getString(); }

    /// Returns the location of this reference.
    Location getLocation() const { return loc; }

    /// Extends this reference with another declaration.
    void addDeclaration(SubroutineDecl *srDecl);

    /// Returns true if this references more than one declaration.
    bool isOverloaded() const { return numDeclarations() > 1; }

    /// Returns true if this reference is empty.
    bool empty() const { return numDeclarations() == 0; }

    /// Returns true if this is a reference to a set of function declarations.
    bool referencesFunctions() const {
        return empty() ? false : llvm::isa<FunctionDecl>(decls[0]);
    }

    /// Returns true if this is a reference to a set of procedure declarations.
    bool referencesProcedures() const {
        return empty() ? false : llvm::isa<ProcedureDecl>(decls[0]);
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

    //@{
    /// If this is a resolved reference, return the unique declaration it
    /// names.  Otherwise, return null.
    const SubroutineDecl *getDeclaration() const {
        return isResolved() ? decls[0] : 0;
    }

    SubroutineDecl *getDeclaration() {
        return isResolved() ? decls[0] : 0;
    }
    //@}

    /// Returns true if this reference contains the given subroutine
    /// declaration.
    bool contains(const SubroutineDecl *srDecl) const;

    /// Returns true if this reference contains a subroutine declaration with
    /// the given type.
    bool contains(const SubroutineType *srType) const;

    /// Removes all subroutine declarations which are not of the given arity
    /// from this reference.
    ///
    /// Returns true if any declarations remain after the filtering and false
    /// if this reference was reduced to the empty reference.
    bool keepSubroutinesWithArity(unsigned arity);

    /// Resolves this reference to point at the given declaration.
    ///
    /// This method removes all declarations from the reference, then adds in
    /// the given declaration.  This method is used once overloads have been
    /// resolved.
    void resolve(SubroutineDecl *srDecl) {
        decls.clear();
        decls.push_back(srDecl);
    }

    /// Returns true if this is a resolved reference.
    bool isResolved() const { return numDeclarations() == 1; }

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

        mutable SubroutineRef::iterator I;

        SubroutineDeclIter(SubroutineRef::iterator I)
            : I(I) { }

        /// Returns the underlying iterator.
        SubroutineRef::iterator getIterator() const { return I; }

        friend class SubroutineRef;

    public:
        SubroutineDeclIter(const SubroutineDeclIter<T> &iter)
            : I(iter.I) { }

        T *operator *() {
            return llvm::cast<T>(*I);
        }

        const T *operator *() const {
            return llvm::cast<T>(*I);
        }

        bool operator ==(const SubroutineDeclIter &iter) const {
            return this->I == iter.I;
        }

        bool operator !=(const SubroutineDeclIter &iter) const {
            return !this->operator==(iter);
        }

        SubroutineDeclIter &operator ++() {
            ++I;
            return *this;
        }

        const SubroutineDeclIter &operator ++() const {
            ++I;
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
            return fun_iterator(begin());
        else
            return fun_iterator(end());
    }
    fun_iterator end_functions() {
        return fun_iterator(end());
    }

    typedef const SubroutineDeclIter<FunctionDecl> const_fun_iterator;
    const_fun_iterator begin_functions() const {
        SubroutineRef *ref = const_cast<SubroutineRef*>(this);
        if (this->referencesFunctions())
            return const_fun_iterator(ref->begin());
        else
            return const_fun_iterator(ref->end());
    }
    const_fun_iterator end_functions() const {
        SubroutineRef *ref = const_cast<SubroutineRef*>(this);
        return const_fun_iterator(ref->end());
    }

    typedef SubroutineDeclIter<ProcedureDecl> proc_iterator;
    proc_iterator begin_procedures() {
        if (this->referencesProcedures())
            return proc_iterator(begin());
        else
            return proc_iterator(end());
    }
    proc_iterator end_procedures() {
        return proc_iterator(end());
    }

    typedef const SubroutineDeclIter<ProcedureDecl> const_proc_iterator;
    const_proc_iterator begin_procedures() const {
        SubroutineRef *ref = const_cast<SubroutineRef*>(this);
        if (this->referencesProcedures())
            return const_proc_iterator(ref->begin());
        else
            return const_proc_iterator(ref->end());
    }
    const_proc_iterator end_procedures() const {
        SubroutineRef *ref = const_cast<SubroutineRef*>(this);
        return const_proc_iterator(ref->end());
    }
    //@}

    /// \name Erase Methods.
    ///
    /// \brief Removes the declaration pointed to by the given iterator from
    /// this reference.
    ///
    /// Returns an iterator pointing to the next declaration in the reference.
    //@{
    iterator erase(iterator I) { return decls.erase(I); }

    template <class T>
    SubroutineDeclIter<T> erase(SubroutineDeclIter<T> SDI) {
        iterator I = SDI.getIterator();
        I = decls.erase(I);
        return SubroutineDeclIter<T>(I);
    }
    //@}

    static bool classof(const SubroutineRef *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_SubroutineRef;
    }

private:
    DeclVector decls;
    Location loc;

    /// Used to check that the current set of declarations are named
    /// and typed consitently.
    void verify();

    /// Asserts that the given subroutine decl has the given name and that it
    /// denotes a function or procedure.  The latter check being governed by the
    /// value of \p isaFunction.
    void verify(SubroutineDecl *decl, IdentifierInfo *name, bool isaFunction);
};

} // end comma namespace.

#endif
