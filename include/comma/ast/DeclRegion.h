//===-- ast/DeclRegion.h -------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_DECLREGION_HDR_GUARD
#define COMMA_AST_DECLREGION_HDR_GUARD

#include "comma/ast/AstBase.h"

#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/SmallVector.h"

#include <list>
#include <vector>

namespace comma {

//===----------------------------------------------------------------------===//
// DeclRegion
class DeclRegion {

protected:
    DeclRegion(Ast::AstKind kind)
        : regionKind(kind), parent(0) { }

    DeclRegion(Ast::AstKind kind, DeclRegion *parent)
        : regionKind(kind), parent(parent) { }

    typedef std::vector<Decl*> DeclarationTable;
    DeclarationTable declarations;

public:
    DeclRegion *getParent() { return parent; }
    const DeclRegion *getParent() const { return parent; }

    // Sets the parent of this region.  This function can only be called if the
    // parent of this region has not yet been set.
    void setParent(DeclRegion *parentRegion) {
        assert(!parent && "Cannot reset the parent of a DeclRegion!");
        parent = parentRegion;
    }

    // Adds the given decl to this region.  If a decl with the same name already
    // exists in this region that previous decl is unconditionally overwritten.
    // It is the responsibility of the caller to ensure that this operation is
    // semantically valid.
    void addDecl(Decl *decl);

    // Returns the number of declarations in this region.
    unsigned countDecls() const { return declarations.size(); }

    //@{
    /// Returns the i'th declaration provided by this region.
    const Decl *getDecl(unsigned i) const {
        assert(i < countDecls() && "Index out of range!");
        return declarations[i];
    }
    Decl *getDecl(unsigned i) {
        assert(i < countDecls() && "Index out of range!");
        return declarations[i];
    }
    //@}

    typedef DeclarationTable::iterator DeclIter;
    DeclIter beginDecls() { return declarations.begin(); }
    DeclIter endDecls()   { return declarations.end(); }

    typedef DeclarationTable::const_iterator ConstDeclIter;
    ConstDeclIter beginDecls() const { return declarations.begin(); }
    ConstDeclIter endDecls()   const { return declarations.end(); }

    typedef DeclarationTable::reverse_iterator reverse_decl_iter;
    reverse_decl_iter rbegin_decls() { return declarations.rbegin(); }
    reverse_decl_iter rend_decls()   { return declarations.rend(); }

    typedef DeclarationTable::const_reverse_iterator const_reverse_decl_iter;
    const_reverse_decl_iter rbegin_decls() const { return declarations.rbegin(); }
    const_reverse_decl_iter rend_decls() const { return declarations.rend(); }

    //===------------------------------------------------------------------===//
    // PredIter
    //
    // FIXME: This class should be lifted out to its own header, likely
    // generalized to a template.  We leave it here for now until it is proved
    // generally useful/desirable.
    //
    // A PredIter is a trivial forward iterator which skips certain elements of
    // the underlying sequence according to a supplied predicate object.
    //
    // Most implementations of such "filter" iterators use templates to forward
    // the predicate and underlying iterator types.  This implementation relies
    // on the PredIter::Predicate structure to supply a generic interface to the
    // predicate object thru a virtual operator ().  PredIter::Predicate is
    // itself a subclass of llvm::RefCountedBaseVPTR, meaning that instances of
    // this predicate class are managed by an internal reference count.  When a
    // PredIter is constructed, the ownership of the supplied Predicate object
    // is released to the PredIter.
    //
    // This organization simplifies the type interface by eliminating template
    // parameters for each predicate yet permits arbitrary filtering strategies
    // to be implemented.
    class PredIter {
        friend class DeclRegion;

        typedef DeclRegion::ConstDeclIter DeclIter;

        struct Predicate : public llvm::RefCountedBaseVPTR<Predicate> {
            virtual bool operator()(const Decl*) = 0;
        };

        typedef llvm::IntrusiveRefCntPtr<Predicate> IntrusivePtr;
        IntrusivePtr predicate;
        DeclIter     cursor;
        DeclIter     end;

        bool predCall(const Decl *decl) {
            return predicate->operator()(decl);
        }

        PredIter(Predicate *pred,
                 DeclIter   begin,
                 DeclIter   end)
            : predicate(pred), cursor(begin), end(end) {
            while (cursor != end) {
                if (predCall(*cursor)) break;
                ++cursor;
            }
        }

        PredIter(DeclIter end)
            : predicate(0), cursor(end), end(end) { }

    public:
        // Returns true if the given iterator points to the same element as this
        // iterator.  Equality is indifferent to the particular predicate.
        bool operator ==(const PredIter& iter) {
            return cursor == iter.cursor;
        }

        bool operator !=(const PredIter& iter) {
            return cursor != iter.cursor;
        }

        PredIter &operator++() {
            if (cursor != end)
                while (++cursor != end) {
                    if (predCall(*cursor)) break;
                }
            return *this;
        }

        PredIter operator++(int) {
            PredIter res = *this;
            if (cursor != end)
                while (++cursor != end) {
                    if (predCall(*cursor)) break;
                }
            return res;
        }

        Decl *operator*() { return *cursor; }
    };

    typedef std::pair<PredIter, PredIter> PredRange;

    PredRange findDecls(IdentifierInfo *name) const;

    // Returns true if this region contains a declaration with the given name.
    bool containsDecl(IdentifierInfo *name) const {
        PredRange range = findDecls(name);
        return range.first != range.second;
    }

    // Returns true if this region contains the given declaration.
    bool containsDecl(const Decl *decl) const;

    // Returns the declaration with the given name and type in this region if
    // one is present, else 0.
    Decl *findDecl(IdentifierInfo *name, Type *type);

    // Removes the given decl.  Returns true if the decl existed and was
    // removed, false otherwise.
    bool removeDecl(Decl *decl);

    // Looks up all function declaration nodes in this region with the given
    // name, pushing the results onto the supplied vector.  Returns true if any
    // declarations were found and false otherwise.
    bool collectFunctionDecls(IdentifierInfo *name,
                              llvm::SmallVectorImpl<SubroutineDecl*> &dst);

    // Looks up all procedure declaration nodes in this region with the given
    // name, pushing the results onto the supplied vector.  Returns true if any
    // declarations were found and false otherwise.
    bool collectProcedureDecls(IdentifierInfo *name,
                               llvm::SmallVectorImpl<SubroutineDecl*> &dst);

    // Converts this DeclRegion into a raw Ast node.
    Ast *asAst();
    const Ast *asAst() const;

    void addObserver(DeclRegion *region) { observers.push_front(region); }

    static bool classof(const Ast *node) {
        switch (node->getKind()) {
        default:
            return false;
        case Ast::AST_DomainInstanceDecl:
        case Ast::AST_AbstractDomainDecl:
        case Ast::AST_PercentDecl:
        case Ast::AST_AddDecl:
        case Ast::AST_ProcedureDecl:
        case Ast::AST_FunctionDecl:
        case Ast::AST_EnumerationDecl:
        case Ast::AST_IntegerDecl:
        case Ast::AST_ArrayDecl:
        case Ast::AST_RecordDecl:
        case Ast::AST_BlockStmt:
            return true;
        }
    }

    static bool classof(const AddDecl       *node) { return true; }
    static bool classof(const ProcedureDecl *node) { return true; }
    static bool classof(const FunctionDecl  *node) { return true; }
    static bool classof(const BlockStmt     *node) { return true; }
    static bool classof(const IntegerDecl   *node) { return true; }
    static bool classof(const PercentDecl   *node) { return true; }
    static bool classof(const RecordDecl    *node) { return true; }
    static bool classof(const ArrayDecl     *node) { return true; }
    static bool classof(const DomainInstanceDecl *node) { return true; }
    static bool classof(const AbstractDomainDecl *node) { return true; }
    static bool classof(const EnumerationDecl    *node) { return true; }

protected:
    virtual void notifyAddDecl(Decl *decl);
    virtual void notifyRemoveDecl(Decl *decl);

    /// \brief Adds the declarations from the given region to this one using the
    /// supplied rewrite rules.
    ///
    /// This method is intended to support subclasses which have some subset of
    /// their declarative regions defined in terms of another declaration.  For
    /// example, domain instances are a rewritten version of the domains
    /// PercentDecl.
    void addDeclarationsUsingRewrites(DeclRewriter &rewrites,
                                      const DeclRegion *region);

    /// \brief Adds the given declaration to this region using the supplied
    /// rewrite rules.
    void addDeclarationUsingRewrites(DeclRewriter &rewrites, Decl *decl);

private:
    Ast::AstKind regionKind;
    DeclRegion *parent;

    typedef std::list<DeclRegion*> ObserverList;
    ObserverList observers;

    void notifyObserversOfAddition(Decl *decl);
    void notifyObserversOfRemoval(Decl *decl);
};

} // End comma namespace.

namespace llvm {

// Specialize isa_impl_wrap to test if a DeclRegion is a specific Ast.
template<class To>
struct isa_impl_wrap<To,
                     const comma::DeclRegion, const comma::DeclRegion> {
    static bool doit(const comma::DeclRegion &val) {
        return To::classof(val.asAst());
    }
};

template<class To>
struct isa_impl_wrap<To, comma::DeclRegion, comma::DeclRegion>
  : public isa_impl_wrap<To,
                         const comma::DeclRegion,
                         const comma::DeclRegion> { };

// Ast to DeclRegion conversions.
template<class From>
struct cast_convert_val<comma::DeclRegion, From, From> {
    static comma::DeclRegion &doit(const From &val) {
        return *val.asDeclRegion();
    }
};

template<class From>
struct cast_convert_val<comma::DeclRegion, From*, From*> {
    static comma::DeclRegion *doit(const From *val) {
        return val->asDeclRegion();
    }
};

template<class From>
struct cast_convert_val<const comma::DeclRegion, From, From> {
    static const comma::DeclRegion &doit(const From &val) {
        return *val.asDeclRegion();
    }
};

template<class From>
struct cast_convert_val<const comma::DeclRegion, From*, From*> {
    static const comma::DeclRegion *doit(const From *val) {
        return val->asDeclRegion();
    }
};

// DeclRegion to Ast conversions.
template<class To>
struct cast_convert_val<To,
                        const comma::DeclRegion,
                        const comma::DeclRegion> {
    static To &doit(const comma::DeclRegion &val) {
        return *reinterpret_cast<To*>(
            const_cast<comma::Ast*>(val.asAst()));
    }
};

template<class To>
struct cast_convert_val<To, comma::DeclRegion, comma::DeclRegion>
    : public cast_convert_val<To,
                              const comma::DeclRegion,
                              const comma::DeclRegion> { };

template<class To>
struct cast_convert_val<To,
                        const comma::DeclRegion*,
                        const comma::DeclRegion*> {
    static To *doit(const comma::DeclRegion *val) {
        return reinterpret_cast<To*>(
            const_cast<comma::Ast*>(val->asAst()));
    }
};

template<class To>
struct cast_convert_val<To, comma::DeclRegion*, comma::DeclRegion*>
    : public cast_convert_val<To,
                              const comma::DeclRegion*,
                              const comma::DeclRegion*> { };

} // End llvm namespace.

#endif
