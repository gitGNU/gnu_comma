//===-- typecheck/CheckName.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "Scope.h"
#include "TypeCheck.h"
#include "comma/ast/DiagPrint.h"
#include "comma/ast/ExceptionRef.h"
#include "comma/ast/Expr.h"
#include "comma/ast/KeywordSelector.h"
#include "comma/ast/PackageRef.h"
#include "comma/ast/Stmt.h"
#include "comma/ast/TypeRef.h"
#include "comma/basic/Attributes.h"

using namespace comma;
using llvm::cast_or_null;
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

Ast *TypeCheck::checkIndirectName(Location loc, Resolver &resolver)
{
    // Check if there is a unique indirect type.
    if (resolver.hasVisibleIndirectType()) {
        TypeDecl *tdecl = resolver.getIndirectType(0);
        return new TypeRef(loc, tdecl);
    }

    // Check if there are any indirect functions.
    if (resolver.hasVisibleIndirectOverloads()) {
        if (SubroutineRef *ref = buildSubroutineRef(loc, resolver))
            return ref;
        else {
            report(loc, diag::NAME_NOT_VISIBLE) << resolver.getIdInfo();
            return 0;
        }
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
    return 0;
}

Node TypeCheck::acceptDirectName(IdentifierInfo *name, Location loc,
                                 bool forStatement)
{
    if (Ast *result = checkDirectName(name, loc, forStatement))
        return getNode(result);
    else
        return getInvalidNode();
}

Ast *TypeCheck::checkDirectName(IdentifierInfo *name, Location loc,
                                bool forStatement)
{
    Resolver &resolver = scope.getResolver();

    if (!resolver.resolve(name)) {
        report(loc, diag::NAME_NOT_VISIBLE) << name;
        return 0;
    }

    // If there is a direct value, it shadows all other names.
    if (resolver.hasDirectValue()) {
        ValueDecl *vdecl = resolver.getDirectValue();
        return new DeclRefExpr(vdecl, loc);
    }

    // If there is a direct type, it shadows all other names.
    if (resolver.hasDirectType()) {
        TypeDecl *tdecl = resolver.getDirectType();
        return new TypeRef(loc, tdecl);
    }

    // If there is a direct exception, it shadows all other names.
    if (resolver.hasDirectException()) {
        ExceptionDecl *edecl = resolver.getDirectException();
        return new ExceptionRef(loc, edecl);
    }

    // Ditto for direct packages.
    if (resolver.hasDirectPackage()) {
        PackageDecl *package = resolver.getDirectPackage();
        return new PackageRef(loc, package->getInstance());
    }

    // Filter the subroutines depending on what kind of name we are trying to
    // form.
    if (forStatement)
        resolver.filterFunctionals();
    else
        resolver.filterProcedures();

    // If there are direct subroutines (possibly several), build a SubroutineRef
    // to encapsulate them.
    if (resolver.hasDirectOverloads()) {
        if (SubroutineRef *ref = buildSubroutineRef(loc, resolver))
            return ref;
        else {
            report(loc, diag::NAME_NOT_VISIBLE) << name;
            return 0;
        }
    }

    // There are no visible direct names, resolve any indirect names.
    return checkIndirectName(loc, resolver);
}

Node TypeCheck::acceptCharacterLiteral(IdentifierInfo *lit, Location loc)
{
    // We treat character literals as simple functions with funny names.
    return acceptDirectName(lit, loc, false);
}

Ast *TypeCheck::processExpandedName(DeclRegion *region,
                                    IdentifierInfo *name, Location loc,
                                    bool forStatement)
{
    // Scan the entire set of declaration nodes, matching against the components
    // with the given name.  In addition, look for enumeration declarations
    // which in turn provide a literal of the given name.  This allows the
    // "short hand qualification" of enumeration literals.  For example, given:
    //
    //   package P is type T is (X); end P;
    //
    // then the qualified name P.X will resolve to P.T.X.  Note however that the
    // two forms are not equivalent, as the former will match all functions
    // named X declared in P (as well as other enumeration literals of the same
    // name).
    llvm::SmallVector<SubroutineDecl*, 8> overloads;

    for (DeclRegion::DeclIter I = region->beginDecls(), E = region->endDecls();
         I != E; ++I) {
        Decl *decl = *I;

        if (decl->getIdInfo() == name) {
            // DeclRegions provide at most a single type with the given name or
            // a set of overloaded subroutines (we do not yet support values at
            // this time).
            if (TypeDecl *tdecl = dyn_cast<TypeDecl>(decl)) {
                assert(overloads.empty() && "Inconsistent DeclRegion!");
                return new TypeRef(loc, tdecl);
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

    if (overloads.empty()) {
        report(loc, diag::NAME_NOT_VISIBLE) << name;
        return 0;
    }

    return new SubroutineRef(loc, &overloads[0], overloads.size());
}

Ast *TypeCheck::processSelectedComponent(Expr *expr,
                                         IdentifierInfo *name, Location loc,
                                         bool forStatement)
{
    // If the prefix expression does not have a resolved type we must wait for
    // the top down pass.  Construct an ambiguous selectedExpr node and return.
    if (!expr->hasResolvedType())
        return new SelectedExpr(expr, name, loc);

    RecordType *prefixTy;
    Type *exprTy = resolveType(expr->getType());
    bool requiresDereference = false;

    if (!(prefixTy = dyn_cast<RecordType>(exprTy))) {
        exprTy = getCoveringDereference(exprTy, Type::CLASS_Record);
        prefixTy = cast_or_null<RecordType>(exprTy);
        requiresDereference = prefixTy != 0;
    }

    if (!prefixTy) {
        report(expr->getLocation(), diag::INVALID_PREFIX_FOR_COMPONENT) << name;
        return 0;
    }

    // Locate the named component in the corresponding record declaration.
    RecordDecl *recordDecl = prefixTy->getDefiningDecl();
    ComponentDecl *component = recordDecl->getComponent(name);
    if (!component) {
        report(loc, diag::UNKNOWN_SELECTED_COMPONENT)
            << name << recordDecl->getIdInfo();
        return 0;
    }

    if (requiresDereference)
        expr = implicitlyDereference(expr, prefixTy);
    return new SelectedExpr(expr, component, loc, component->getType());
}

Node TypeCheck::acceptSelectedComponent(Node prefix,
                                        IdentifierInfo *name, Location loc,
                                        bool forStatement)
{
    Ast *result = 0;

    if (TypeRef *Tyref = lift_node<TypeRef>(prefix)) {
        DeclRegion *region = Tyref->getDecl()->asDeclRegion();
        result = processExpandedName(region, name, loc, forStatement);
    }
    else if (PackageRef *Pref = lift_node<PackageRef>(prefix)) {
        PkgInstanceDecl *pkg = Pref->getPackageInstance();
        result = processExpandedName(pkg, name, loc, forStatement);
    }
    else if (Expr *expr = lift_node<Expr>(prefix))
        result = processSelectedComponent(expr, name, loc, forStatement);
    else {
        report(loc, diag::INVALID_PREFIX_FOR_COMPONENT) << name;
        result = 0;
    }

    if (result) {
        prefix.release();
        return getNode(result);
    }
    else
        return getInvalidNode();
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
        return getNode(selector);
    }

    // Otherwise the rhs must be a procedure call, which is not valid.
    ProcedureCallStmt *call = cast_node<ProcedureCallStmt>(rhs);
    report(call->getLocation(), diag::INVALID_CONTEXT_FOR_PROCEDURE);
    return getInvalidNode();
}

Node TypeCheck::acceptApplication(Node prefix, NodeVector &argNodes)
{
    Ast *result = 0;

    // There are three cases (currently):
    //
    //   - The prefix is a SubroutineRef naming a family of subroutines.
    //
    //   - The prefix is a TypeRef (type conversion).
    //
    //   - The prefix is an Expr resolving to an object of ArrayType.
    //
    if (SubroutineRef *ref = lift_node<SubroutineRef>(prefix))
        result = acceptSubroutineApplication(ref, argNodes);
    else if (TypeRef *ref = lift_node<TypeRef>(prefix))
        result = acceptConversionExpr(ref, argNodes);
    else if (Expr *expr = lift_node<Expr>(prefix))
        result = acceptIndexedArray(expr, argNodes);
    else {
        assert(false && "Bad prefix node!");
        result = 0;
    }

    if (result) {
        prefix.release();
        argNodes.release();
        return getNode(result);
    }
    else
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

Ast *TypeCheck::finishName(Ast *node)
{
    // This method is called to "complete" a name.  If the name is a
    // SubroutineRef, it must contain at least one nullary subroutine.
    if (SubroutineRef *ref = dyn_cast<SubroutineRef>(node))
        return finishSubroutineRef(ref);

    // FIXME: Perhaps we are being too permissive here.  We should assert that
    // the node is in fact a name.
    return node;
}

Node TypeCheck::finishName(Node node)
{
    Ast *name = cast_node<Ast>(node);
    if (Ast *result = finishName(name)) {
        node.release();
        return getNode(result);
    }
    return getInvalidNode();
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

    // For functions there is nothing to typecheck until we hit the topdown pass
    // and verify the return type.  Procedures, on the other hand, must resolve
    // uniquely.
    if (ref->referencesFunctions())
        return new FunctionCallExpr(ref, 0, 0, 0, 0);
    else if (ref->isResolved())
        return new ProcedureCallStmt(ref, 0, 0, 0, 0);
    else {
        report(loc, diag::AMBIGUOUS_EXPRESSION);
        for (SubroutineRef::iterator I = ref->begin(); I != ref->end(); ++I)
            report(loc, diag::CANDIDATE_NOTE) << diag::PrintDecl(*I);
        return 0;
    }
}

