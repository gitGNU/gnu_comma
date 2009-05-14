//===-- ast/DeclarativeRegion.h ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_DECLARATIVEREGION_HDR_GUARD
#define COMMA_DECLARATIVEREGION_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include <list>
#include <vector>

namespace comma {

//===----------------------------------------------------------------------===//
// DeclarativeRegion
class DeclarativeRegion {

protected:
    DeclarativeRegion(Ast::AstKind kind)
        : regionKind(kind), parent(0) { }

    DeclarativeRegion(Ast::AstKind kind, DeclarativeRegion *parent)
        : regionKind(kind), parent(parent) { }

    typedef std::vector<Decl*> DeclarationTable;
    DeclarationTable declarations;

public:
    DeclarativeRegion *getParent() { return parent; }
    const DeclarativeRegion *getParent() const { return parent; }

    // Sets the parent of this region.  This function can only be called if the
    // parent of this region has not yet been set.
    void setParent(DeclarativeRegion *parentRegion) {
        assert(!parent && "Cannot reset the parent of a DeclarativeRegion!");
        parent = parentRegion;
    }

    // Adds the given decl to this region.  If a decl with the same name already
    // exists in this region that previous decl is unconditionally overwritten.
    // It is the responsibility of the caller to ensure that this operation is
    // semantically valid.
    void addDecl(Decl *decl);

    // Adds the given declaration to the region using the supplied rewrite
    // rules.
    void addDeclarationUsingRewrites(const AstRewriter &rewrites,
                                     Decl *decl);

    // Adds the declarations from the given region to this one using the
    // supplied rewrite rules.
    void addDeclarationsUsingRewrites(const AstRewriter       &rewrites,
                                      const DeclarativeRegion *region);

    typedef DeclarationTable::iterator DeclIter;
    DeclIter beginDecls() { return declarations.begin(); }
    DeclIter endDecls()   { return declarations.end(); }

    typedef DeclarationTable::const_iterator ConstDeclIter;
    ConstDeclIter beginDecls() const { return declarations.begin(); }
    ConstDeclIter endDecls()   const { return declarations.end(); }

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
        friend class DeclarativeRegion;

        typedef DeclarativeRegion::ConstDeclIter DeclIter;

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

    // Returns the declaration with the given name and type in this region if
    // one is present, else 0.
    Decl *findDecl(IdentifierInfo *name, Type *type);

    // Removes the given decl.  Returns true if the decl existed and was
    // removed, false otherwise.
    bool removeDecl(Decl *decl);

    // Looks up all function declaration nodes in this region with the given
    // name and arity, pushing the results onto the supplied vector.  Returns
    // true if any declarations were found and false otherwise.
    bool collectFunctionDecls(IdentifierInfo *name,
                              unsigned        arity,
                              std::vector<SubroutineDecl*> &dst);


    // Looks up all procedure declaration nodes in this region with the given
    // name and arity, pushing the results onto the supplied vector.  Returns
    // true if any declarations were found and false otherwise.
    bool collectProcedureDecls(IdentifierInfo *name,
                               unsigned        arity,
                               std::vector<SubroutineDecl*> &dst);

    // Converts this DeclarativeRegion into a raw Ast node.
    Ast *asAst();
    const Ast *asAst() const;

    void addObserver(DeclarativeRegion *region) { observers.push_front(region); }

    static bool classof(const Ast *node) {
        switch (node->getKind()) {
        default:
            return false;
        case Ast::AST_DomainDecl:
        case Ast::AST_SignatureDecl:
        case Ast::AST_VarietyDecl:
        case Ast::AST_FunctorDecl:
        case Ast::AST_DomainInstanceDecl:
        case Ast::AST_AbstractDomainDecl:
        case Ast::AST_AddDecl:
        case Ast::AST_ProcedureDecl:
        case Ast::AST_FunctionDecl:
        case Ast::AST_EnumerationDecl:
        case Ast::AST_BlockStmt:
            return true;
        }
    }

    static bool classof(const DomainDecl    *node) { return true; }
    static bool classof(const SignatureDecl *node) { return true; }
    static bool classof(const VarietyDecl   *node) { return true; }
    static bool classof(const FunctorDecl   *node) { return true; }
    static bool classof(const AddDecl       *node) { return true; }
    static bool classof(const ProcedureDecl *node) { return true; }
    static bool classof(const FunctionDecl  *node) { return true; }
    static bool classof(const BlockStmt     *node) { return true; }
    static bool classof(const DomainInstanceDecl *node) { return true; }
    static bool classof(const AbstractDomainDecl *node) { return true; }
    static bool classof(const EnumerationDecl    *node) { return true; }

protected:
    virtual void notifyAddDecl(Decl *decl);
    virtual void notifyRemoveDecl(Decl *decl);

private:
    Ast::AstKind       regionKind;
    DeclarativeRegion *parent;

    typedef std::list<DeclarativeRegion*> ObserverList;
    ObserverList observers;

    void notifyObserversOfAddition(Decl *decl);
    void notifyObserversOfRemoval(Decl *decl);
};

} // End comma namespace.

namespace llvm {

// Specialize isa_impl_wrap to test if a DeclarativeRegion is a specific Decl.
template<class To>
struct isa_impl_wrap<To,
                     const comma::DeclarativeRegion, const comma::DeclarativeRegion> {
    static bool doit(const comma::DeclarativeRegion &val) {
        return To::classof(val.asAst());
    }
};

template<class To>
struct isa_impl_wrap<To, comma::DeclarativeRegion, comma::DeclarativeRegion>
  : public isa_impl_wrap<To,
                         const comma::DeclarativeRegion,
                         const comma::DeclarativeRegion> { };

// Decl to DeclarativeRegion conversions.
template<class From>
struct cast_convert_val<comma::DeclarativeRegion, From, From> {
    static comma::DeclarativeRegion &doit(const From &val) {
        return *val.asDeclarativeRegion();
    }
};

template<class From>
struct cast_convert_val<comma::DeclarativeRegion, From*, From*> {
    static comma::DeclarativeRegion *doit(const From *val) {
        return val->asDeclarativeRegion();
    }
};

template<class From>
struct cast_convert_val<const comma::DeclarativeRegion, From, From> {
    static const comma::DeclarativeRegion &doit(const From &val) {
        return *val.asDeclarativeRegion();
    }
};

template<class From>
struct cast_convert_val<const comma::DeclarativeRegion, From*, From*> {
    static const comma::DeclarativeRegion *doit(const From *val) {
        return val->asDeclarativeRegion();
    }
};

// DeclarativeRegion to Decl conversions.
template<class To>
struct cast_convert_val<To,
                        const comma::DeclarativeRegion,
                        const comma::DeclarativeRegion> {
    static To &doit(const comma::DeclarativeRegion &val) {
        return *reinterpret_cast<To*>(
            const_cast<comma::Ast*>(val.asAst()));
    }
};

template<class To>
struct cast_convert_val<To, comma::DeclarativeRegion, comma::DeclarativeRegion>
    : public cast_convert_val<To,
                              const comma::DeclarativeRegion,
                              const comma::DeclarativeRegion> { };

template<class To>
struct cast_convert_val<To,
                        const comma::DeclarativeRegion*,
                        const comma::DeclarativeRegion*> {
    static To *doit(const comma::DeclarativeRegion *val) {
        return reinterpret_cast<To*>(
            const_cast<comma::Ast*>(val->asAst()));
    }
};

template<class To>
struct cast_convert_val<To, comma::DeclarativeRegion*, comma::DeclarativeRegion*>
    : public cast_convert_val<To,
                              const comma::DeclarativeRegion*,
                              const comma::DeclarativeRegion*> { };

} // End llvm namespace.

#endif
