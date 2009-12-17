//===-- typecheck/CheckName.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "Scope.h"
#include "TypeCheck.h"
#include "comma/ast/ExceptionRef.h"
#include "comma/ast/Expr.h"
#include "comma/ast/KeywordSelector.h"
#include "comma/ast/Stmt.h"
#include "comma/ast/TypeRef.h"
#include "comma/basic/Attributes.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

namespace {

/// Utility routine for building SubroutineRef nodes using the subroutine
/// declarations provided by the given Resolver.
SubroutineRef *buildSubroutineRef(Location loc, Resolver &resolver)
{
    llvm::SmallVector<SubroutineDecl *, 8> routines;
    resolver.getVisibleSubroutines(routines);

    if (routines.empty())
        return 0;
    return new SubroutineRef(loc, &routines[0], routines.size());
}

} // end anonymous namespace.

Node TypeCheck::acceptIndirectName(Location loc, Resolver &resolver)
{
    // Check if there is a unique indirect type.
    if (resolver.hasVisibleIndirectType()) {
        TypeDecl *tdecl = resolver.getIndirectType(0);
        TypeRef *ref = new TypeRef(loc, tdecl);
        return getNode(ref);
    }

    // Check if there are any indirect functions.
    if (resolver.hasVisibleIndirectOverloads()) {
        SubroutineRef *ref = buildSubroutineRef(loc, resolver);
        if (!ref) {
            report(ref->getLocation(), diag::NAME_NOT_VISIBLE)
                << resolver.getIdInfo();
            return getInvalidNode();
        }
        return getNode(ref);
    }

    // Currently, we do not support indirect values.
    assert(!resolver.hasIndirectValues() &&
           "Indirect values are not implemented!");

    // Otherwise, there is an ambiguity and the name requires qualification.
    //
    // FIXME:  The resolver contains the full set of indirect declarations.  The
    // intent is to make that information available for the sake of detailed
    // diagnostics.  Use it here.
    report(loc, diag::NAME_REQUIRES_QUAL) << resolver.getIdInfo();
    return getInvalidNode();
}

Node TypeCheck::acceptDirectName(IdentifierInfo *name, Location loc,
                                 bool forStatement)
{
    Resolver &resolver = scope.getResolver();

    if (!resolver.resolve(name)) {
        report(loc, diag::NAME_NOT_VISIBLE) << name;
        return getInvalidNode();
    }

    // If there is a direct value, it shadows all other names.
    if (resolver.hasDirectValue()) {
        ValueDecl *vdecl = resolver.getDirectValue();
        DeclRefExpr *ref = new DeclRefExpr(vdecl, loc);
        return getNode(ref);
    }

    // If there is a direct type, it shadows all other names.  Note that we do
    // not support parameterized types (as opposed to varieties and functors)
    // yet.
    if (resolver.hasDirectType()) {
        TypeDecl *tdecl = resolver.getDirectType();
        TypeRef *ref = new TypeRef(loc, tdecl);
        return getNode(ref);
    }

    // If there is a direct exception, it shadows all other names.
    if (resolver.hasDirectException()) {
        ExceptionDecl *edecl = resolver.getDirectException();
        ExceptionRef *ref = new ExceptionRef(loc, edecl);
        return getNode(ref);
    }

    // If a direct capsule (which currently means Domoid's and Sigoid's) is
    // visible, we distinguish between parameterized and non-parameterized
    // cases.  For the former case, return a TypeRef to the unique instance.
    // For the latter case, form an incomplete TypeRef to the functor or
    // variety.
    if (resolver.hasDirectCapsule()) {
        ModelDecl *mdecl = resolver.getDirectCapsule();
        TypeRef *ref = buildTypeRefForModel(loc, mdecl);
        return getNode(ref);
    }

    // Filter the procedures according depending on what kind of name we are
    // trying to form.
    if (forStatement)
        resolver.filterFunctionals();
    else
        resolver.filterProcedures();

    // If there are direct subroutines (possibly several), build a SubroutineRef
    // to encapsulate them.
    if (resolver.hasDirectOverloads()) {
        SubroutineRef *ref = buildSubroutineRef(loc, resolver);
        if (!ref) {
            report(loc, diag::NAME_NOT_VISIBLE) << name;
            return getInvalidNode();
        }
        return getNode(ref);
    }

    // There are no visible direct names, resolve any indirect names.
    return acceptIndirectName(loc, resolver);
}

TypeRef *TypeCheck::buildTypeRefForModel(Location loc, ModelDecl *mdecl)
{
    TypeRef *ref = 0;

    switch (mdecl->getKind()) {

    default:
        assert(false && "Bad kind of model!");
        break;

    case Ast::AST_DomainDecl: {
        DomainDecl *dom = cast<DomainDecl>(mdecl);
        ref = new TypeRef(loc, dom->getInstance());
        break;
    }

    case Ast::AST_SignatureDecl: {
        SignatureDecl *sig = cast<SignatureDecl>(mdecl);
        ref = new TypeRef(loc, sig->getInstance());
        break;
    }

    case Ast::AST_FunctorDecl:
        ref = new TypeRef(loc, cast<FunctorDecl>(mdecl));
        break;

    case Ast::AST_VarietyDecl:
        ref = new TypeRef(loc, cast<VarietyDecl>(mdecl));
        break;

    };
    return ref;
}


Node TypeCheck::acceptCharacterLiteral(IdentifierInfo *lit, Location loc)
{
    // We treat character literals as simple functions with funny names.
    return acceptDirectName(lit, loc, false);
}

Node TypeCheck::acceptSelectedComponent(Node prefix,
                                        IdentifierInfo *name, Location loc,
                                        bool forStatement)
{
    // We do not support records yet, so we only need to consider the case of
    // qualification.  The prefix must therfore be a TypeRef.
    TypeRef *ref = lift_node<TypeRef>(prefix);

    // Currently, a signature can never be used as the prefix to a component.
    if (!ref || ref->referencesSigInstance()) {
        report(loc, diag::INVALID_PREFIX_FOR_COMPONENT) << name;
        return getInvalidNode();
    }

    // Ensure the parser called finishName on the prefix.
    assert(ref->isComplete() && "Invalid prefix node!");

    // All complete TypeRef's (with the expection of SigInstanceDecl's) point to
    // a decl which is also a DeclRegion.
    DeclRegion *region = ref->getDecl()->asDeclRegion();
    assert(region && "Prefix is not a DeclRegion!");

    // Scan the entire set of declaration nodes, matching against the components
    // with the given name.  In addition, look for enumeration declarations
    // which in turn provide a literal of the given name.  This allows the
    // "short hand qualification" of enumeration literals.  For example, given:
    //
    //   domain D with type T is (X); end D;
    //
    // The the qualified name D.X will resolve to D.T.X.  Note however that the
    // two forms are not equivalent, as the former will match all functions
    // named X declared in D (as well as other enumeration literals of the same
    // name).
    DeclRegion::DeclIter I = region->beginDecls();
    DeclRegion::DeclIter E = region->endDecls();

    llvm::SmallVector<SubroutineDecl*, 8> overloads;
    for ( ; I != E; ++I) {
        Decl *decl = *I;

        if (decl->getIdInfo() == name) {
            // DeclRegions provide at most a single type with the given name or
            // a set of overloaded subroutines (we do not yet support values at
            // this time).
            if (TypeDecl *tdecl = dyn_cast<TypeDecl>(decl)) {
                assert(overloads.empty() && "Inconsistent DeclRegion!");
                TypeRef *ref = new TypeRef(loc, tdecl);
                return getNode(ref);
            }

            // Distinguish functions from procedures if we are generiating a
            // name for use as a statement.
            if (forStatement) {
                if (SubroutineDecl *routine = dyn_cast<SubroutineDecl>(decl))
                    overloads.push_back(routine);
            }
            else {
                if (SubroutineDecl *routine = dyn_cast<FunctionDecl>(decl))
                    overloads.push_back(routine);
            }
        }
        else if (EnumerationDecl *edecl = dyn_cast<EnumerationDecl>(*I)) {
            if (EnumLiteral *lit = edecl->findLiteral(name))
                overloads.push_back(lit);
        }
    }

    // If the overload set was not empty, we must have resolved something.
    assert(!overloads.empty() && "Could not resolve name!");

    SubroutineRef *srRef =
        new SubroutineRef(loc, &overloads[0], overloads.size());
    return getNode(srRef);
}

Node TypeCheck::acceptParameterAssociation(IdentifierInfo *key, Location loc,
                                           Node rhs)
{
    // Parameter associations can be built with an Expr on the rhs.
    if (Expr *expr = lift_node<Expr>(rhs)) {
        KeywordSelector *selector = new KeywordSelector(key, loc, expr);
        rhs.release();
        return getNode(selector);
    }

    // Or with a TypeRef.
    if (TypeRef *ref = lift_node<TypeRef>(rhs)) {
        KeywordSelector *selector = new KeywordSelector(key, loc, ref);
        rhs.release();
        assert(ref->isComplete() && "Invalid TypeRef!");
        return getNode(selector);
    }

    // Otherwise the rhs must be a procedure call, which is not valid.
    ProcedureCallStmt *call = cast_node<ProcedureCallStmt>(rhs);
    report(call->getLocation(), diag::INVALID_CONTEXT_FOR_PROCEDURE);
    return getInvalidNode();
}

Node TypeCheck::acceptApplication(Node prefix, NodeVector &argNodes)
{
    // There are three cases (currently):
    //
    //   - The prefix is an incomplete TypeRef (a variety or functor).
    //
    //   - The prefix is a SubroutineRef naming a family of subroutines.
    //
    //   - The prefix is an Expr resolving to an object of ArrayType.
    //
    if (SubroutineRef *ref = lift_node<SubroutineRef>(prefix)) {
        if (Ast *call = acceptSubroutineApplication(ref, argNodes)) {
            prefix.release();
            argNodes.release();
            return getNode(call);
        }
        return getInvalidNode();
    }

    if (TypeRef *ref = lift_node<TypeRef>(prefix)) {
        if (TypeRef *tyRef = acceptTypeApplication(ref, argNodes)) {
            prefix.release();
            argNodes.release();
            return getNode(tyRef);
        }
        return getInvalidNode();
    }

    if (Expr *expr = lift_node<Expr>(prefix)) {
        if (IndexedArrayExpr *IAE = acceptIndexedArray(expr, argNodes)) {
            prefix.release();
            argNodes.release();
            return getNode(IAE);
        }
        return getInvalidNode();
    }

    assert(false && "Bad prefix node!");
    return getInvalidNode();
}

bool
TypeCheck::checkSubroutineArgumentNodes(NodeVector &argNodes,
                                        SVImpl<Expr*>::Type &positional,
                                        SVImpl<KeywordSelector*>::Type &keyed)
{
    NodeVector::iterator I = argNodes.begin();
    NodeVector::iterator E = argNodes.end();

    // Grab the positional args.
    for ( ; I != E; ++I) {
        if (Expr *expr = lift_node<Expr>(*I))
            positional.push_back(expr);
        else
            break;
    }

    // Grab the keyword args.
    for ( ; I != E; ++I) {
        if (KeywordSelector *KS = lift_node<KeywordSelector>(*I))
            if (KS->isExprSelector()) {
                keyed.push_back(KS);
                continue;
            }
        break;
    }

    // If we did not consume all of the argument nodes, the iterator must point
    // to some other invalid node.  Diagnose.
    if (I != E) {
        if (ProcedureCallStmt *call = lift_node<ProcedureCallStmt>(*I)) {
            report(call->getLocation(), diag::INVALID_CONTEXT_FOR_PROCEDURE);
        }
        else if (TypeRef *ref = lift_node<TypeRef>(*I)) {
            report(ref->getLocation(), diag::TYPE_CANNOT_DENOTE_VALUE);
        }
        else if (KeywordSelector *KS = lift_node<KeywordSelector>(*I)) {
            assert(KS->isTypeSelector());
            TypeRef *ref = KS->getTypeRef();
            report(ref->getLocation(), diag::TYPE_CANNOT_DENOTE_VALUE);
        }
        else if (ExceptionRef *ref = lift_node<ExceptionRef>(*I)) {
            report(ref->getLocation(), diag::EXCEPTION_CANNOT_DENOTE_VALUE);
        }
        else {
            // FIXME: This diagnostic is too general.
            report(getNodeLoc(*I), diag::NOT_AN_EXPRESSION);
        }
        return false;
    }
    return true;
}

Ast *TypeCheck::acceptSubroutineApplication(SubroutineRef *ref,
                                            NodeVector &argNodes)
{
    IdentifierInfo *refName = ref->getIdInfo();

    llvm::SmallVector<Expr *, 8> posArgs;
    llvm::SmallVector<KeywordSelector *, 8> keyedArgs;
    llvm::SmallVector<FunctionDecl*, 4> interps;

    if (!checkSubroutineArgumentNodes(argNodes, posArgs, keyedArgs))
        return 0;

    if (ref->referencesFunctions() && keyedArgs.empty()) {
        // We are potentially processing a nullary function call followed by an
        // array index.  Collect the set of nullary functions returning an array
        // type with a dimensionality matching the number of positional
        // arguments.
        unsigned numArgs = posArgs.size();
        SubroutineRef::fun_iterator I = ref->begin_functions();
        SubroutineRef::fun_iterator E = ref->end_functions();

        for ( ; I != E; ++I) {
            FunctionDecl *fdecl = *I;
            Type *retTy = fdecl->getReturnType();
            if (fdecl->getArity() != 0)
                continue;
            if (ArrayType *arrTy = dyn_cast<ArrayType>(retTy)) {
                if (arrTy->getRank() == numArgs)
                    interps.push_back(fdecl);
            }
        }

        // Reduce the set of connectives wrt arity.
        if (!ref->keepSubroutinesWithArity(argNodes.size())) {

            // If there are no array interpretations the call cannot be
            // resolved.  Report an arity mismatch.
            if (interps.empty()) {
                report(ref->getLocation(), diag::WRONG_NUM_ARGS_FOR_SUBROUTINE)
                    << refName;
                return 0;
            }

            // If there is a unique array interpretation, form a nullary call
            // expression and continue checking as an indexed array expression.
            if (interps.size() == 1) {
                FunctionCallExpr *call;
                IndexedArrayExpr *iae;
                ref->addDeclaration(interps[0]);
                call = new FunctionCallExpr(ref);
                if (!(iae = acceptIndexedArray(call, posArgs)))
                    delete call;
                return iae;
            }

            // There are several array interpretations.  Form an ambiguous call
            // expression containing them all and wrap with an indexed array
            // expression.
            FunctionCallExpr *call;
            ref->addDeclarations(interps.begin(), interps.end());
            call = new FunctionCallExpr(ref);
            return new IndexedArrayExpr(call, posArgs.data(), posArgs.size());
        }
    }
    else {
        // This call cannot be interpreted as an indexed array expression.
        // Filter wrt arity.
        if (!ref->keepSubroutinesWithArity(argNodes.size())) {
            report(ref->getLocation(), diag::WRONG_NUM_ARGS_FOR_SUBROUTINE)
                << refName;
            return 0;
        }
    }

    return acceptSubroutineCall(ref, posArgs, keyedArgs);
}

bool TypeCheck::checkTypeArgumentNodes(NodeVector &argNodes,
                                       SVImpl<TypeRef*>::Type &positional,
                                       SVImpl<KeywordSelector*>::Type &keyed)
{
    NodeVector::iterator I = argNodes.begin();
    NodeVector::iterator E = argNodes.end();

    // Grab the positional args.
    for ( ; I != E; ++I) {
        if (TypeRef *ref = lift_node<TypeRef>(*I)) {
            assert(ref->isComplete() && "Incomplete TypeRef!");
            positional.push_back(ref);
        }
        else
            break;
    }

    // Grab the keyword args.
    for ( ; I != E; ++I) {
        if (KeywordSelector *KS = lift_node<KeywordSelector>(*I))
            if (KS->isTypeSelector()) {
                assert(KS->getTypeRef()->isComplete() && "Incomplete TypeRef!");
                keyed.push_back(KS);
                continue;
            }
        break;
    }

    // If we did not consume all of the argument nodes, the iterator must point
    // to an Expr, a selector to an Expr, or a ProcedureCall.  Diagnose.
    if (I != E) {
        if (ProcedureCallStmt *call = lift_node<ProcedureCallStmt>(*I)) {
            report(call->getLocation(), diag::INVALID_CONTEXT_FOR_PROCEDURE);
            return false;
        }

        Expr *expr = lift_node<Expr>(*I);
        if (!expr) {
            KeywordSelector *KS = cast_node<KeywordSelector>(*I);
            assert(KS->isExprSelector());
            expr = KS->getExpression();
        }
        report(expr->getLocation(), diag::EXPRESSION_AS_TYPE_PARAM);
        return false;
    }
    return true;
}

TypeRef *TypeCheck::acceptTypeApplication(TypeRef *ref, NodeVector &argNodes)
{
    // If the type reference is already complete, is cannot accept any
    // arguments.
    if (ref->isComplete()) {
        report(ref->getLocation(), diag::WRONG_NUM_ARGS_FOR_TYPE)
            << ref->getIdInfo();
        return 0;
    }

    llvm::SmallVector<TypeRef*, 8> positionalArgs;
    llvm::SmallVector<KeywordSelector*, 8> keyedArgs;
    if (!checkTypeArgumentNodes(argNodes, positionalArgs, keyedArgs))
        return 0;

    return acceptTypeApplication(ref, positionalArgs, keyedArgs);
}

bool TypeCheck::checkArrayIndexNodes(NodeVector &argNodes,
                                     SVImpl<Expr*>::Type &indices)
{
    // Each argument must be an expression.  In particular, keyword selectors
    // are not valid for indexed array expressions.
    NodeVector::iterator I = argNodes.begin();
    NodeVector::iterator E = argNodes.end();
    for ( ; I != E; ++I) {
        if (Expr *expr = lift_node<Expr>(*I)) {
            indices.push_back(expr);
            continue;
        }

        // Otherwise, get the location of the argument and post a diagnostic.
        Location loc;
        if (KeywordSelector *KS = lift_node<KeywordSelector>(*I))
            loc = KS->getLocation();
        else {
            TypeRef *ref = cast_node<TypeRef>(*I);
            loc = ref->getLocation();
        }
        report(loc, diag::INVALID_ARRAY_INDEX);
        return false;
    }
    return true;
}

IndexedArrayExpr *TypeCheck::acceptIndexedArray(Expr *expr,
                                                NodeVector &argNodes)
{
    llvm::SmallVector<Expr*, 4> indices;
    if (!checkArrayIndexNodes(argNodes, indices))
        return 0;
    return acceptIndexedArray(expr, indices);
}

Node TypeCheck::acceptAttribute(Node prefixNode,
                                IdentifierInfo *name, Location loc)
{
    assert(name->getAttributeID() != attrib::UNKNOWN_ATTRIBUTE);

    attrib::AttributeID ID = name->getAttributeID();
    Ast *prefix = cast_node<Ast>(prefixNode);
    Ast *result = checkAttribute(ID, prefix, loc);

    if (result) {
        prefixNode.release();
        return getNode(result);
    }
    return getInvalidNode();
}

Node TypeCheck::finishName(Node name)
{
    // This method is called on all names which are applied to a set of
    // arguments.  We need to consider two cases:
    //
    //   - If the name is a TypeRef, it must be complete (e.g. not name a
    //     variety or functor).
    //
    //   - If the name is a SubroutineRef, it must contain at least one nullary
    //     subroutine.
    if (SubroutineRef *ref = lift_node<SubroutineRef>(name)) {
        if (Ast *call = finishSubroutineRef(ref)) {
            name.release();
            return getNode(call);
        }
        return getInvalidNode();
    }

    if (TypeRef *ref = lift_node<TypeRef>(name)) {
        if (finishTypeRef(ref)) {
            name.release();
            return name;
        }
        return getInvalidNode();
    }
    return name;
}

Ast *TypeCheck::finishSubroutineRef(SubroutineRef *ref)
{
    IdentifierInfo *name = ref->getIdInfo();
    Location loc = ref->getLocation();

    // Reduce the reference to include only those declarations with arity zero
    // and build the appropriate kind of call node.
    if (!ref->keepSubroutinesWithArity(0)) {
        report(loc, diag::NAME_NOT_VISIBLE) << name;
        return 0;
    }

    if (ref->referencesFunctions())
        return new FunctionCallExpr(ref, 0, 0, 0, 0);
    return new ProcedureCallStmt(ref, 0, 0, 0, 0);
}

bool TypeCheck::finishTypeRef(TypeRef *ref)
{
    // If the type is incomplete, we should have been applied to a set of
    // arguments.
    if (ref->isIncomplete()) {
        report(ref->getLocation(), diag::WRONG_NUM_ARGS_FOR_TYPE)
            << ref->getIdInfo();
        return false;
    }
    return true;
}
