//===-- parser/ParseClient.h ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_PARSECLIENT_HDR_GUARD
#define COMMA_PARSECLIENT_HDR_GUARD

#include "comma/basic/ParameterModes.h"
#include "comma/basic/IdentifierInfo.h"
#include "comma/parser/Descriptors.h"

namespace comma {

class ParseClient {

public:
    // The parser does not know about the underlying node representation.  It
    // calls this method when cleaning up after a bad parse.  The implementation
    // need not delete the node as a result of this call -- it might choose to
    // cache it, for instance.
    virtual void deleteNode(Node &node) = 0;

    // Starts the processing of a model.  The supplied Descriptor contains the
    // name and location of the declaration, as is either of kind
    // desc::Signature or desc::Domain.
    virtual void beginModelDeclaration(Descriptor &desc) = 0;

    virtual void endModelDefinition() = 0;

    // Called immediately after a model declaration has been registered.  This
    // call defines a formal parameter of a model.  The parser collects the
    // results of this call into the given descriptor object (and hense, the
    // supplied descriptor contains all previously accumulated arguments) to be
    // finalized in a call to acceptModelDeclaration.
    virtual Node acceptModelParameter(Descriptor     &desc,
                                      IdentifierInfo *formal,
                                      Node            typeNode,
                                      Location        loc) = 0;

    // This call completes the declaration of a model (name and
    // parameterization).
    virtual void acceptModelDeclaration(Descriptor &desc) = 0;

    virtual void beginWithExpression() = 0;
    virtual void endWithExpression() = 0;

    // Called for each super signature of a with expression.
    virtual void acceptWithSupersignature(Node typeNode, Location loc) = 0;

    // Invoked when the parser consumes a carrier declaration.
    virtual void acceptCarrier(IdentifierInfo *name,
                               Node            typeNode,
                               Location        loc) = 0;

    // Called at the begining of an add expression.  The client accepts
    // components of an add expression after this call until endAddExpression is
    // called.
    virtual void beginAddExpression() = 0;

    // Completes an add expression.
    virtual void endAddExpression() = 0;

    virtual void beginSubroutineDeclaration(Descriptor &desc) = 0;

    virtual Node acceptSubroutineParameter(IdentifierInfo *formal,
                                           Location        loc,
                                           Node            typeNode,
                                           ParameterMode   mode) = 0;

    virtual Node acceptSubroutineDeclaration(Descriptor &desc,
                                             bool definitionFollows) = 0;

    /// Begin a subroutine definition, where \p declarationNode is a valid node
    /// returned from ParseClient::acceptSubroutineDeclaration.
    virtual void beginSubroutineDefinition(Node declarationNode) = 0;

    /// Called for each valid statement constituting the body of the current
    /// subroutine (as established by a call to beginSubroutineDefinition).
    virtual void acceptSubroutineStmt(Node stmt) = 0;

    /// Once the body of a subroutine has been parsed, this callback is invoked
    /// to singnal the completion of the definition.
    virtual void endSubroutineDefinition() = 0;

    virtual bool acceptObjectDeclaration(Location        loc,
                                         IdentifierInfo *name,
                                         Node            type,
                                         Node            initializer) = 0;

    virtual Node acceptPercent(Location loc) = 0;

    virtual Node acceptTypeIdentifier(IdentifierInfo *info, Location loc) = 0;

    virtual Node acceptTypeApplication(IdentifierInfo  *connective,
                                       NodeVector      &argumentNodes,
                                       Location        *argumentLocs,
                                       IdentifierInfo **keys,
                                       Location        *keyLocs,
                                       unsigned         numKeys,
                                       Location         loc) = 0;

    // Submits an import from the given type node, and the location of the
    // import reserved word.
    virtual bool acceptImportDeclaration(Node importedType, Location loc) = 0;

    virtual Node acceptKeywordSelector(IdentifierInfo *key,
                                       Location        loc,
                                       Node            exprNode,
                                       bool            forSubroutine) = 0;

    virtual Node acceptDirectName(IdentifierInfo *name, Location loc) = 0;

    virtual Node acceptQualifiedName(Node            qualNode,
                                     IdentifierInfo *name,
                                     Location        loc) = 0;

    virtual Node acceptFunctionCall(IdentifierInfo  *name,
                                    Location         loc,
                                    NodeVector      &args) = 0;

    virtual Node acceptProcedureCall(IdentifierInfo  *name,
                                     Location         loc,
                                     NodeVector      &args) = 0;

    // Called for "inj" expressions.  loc is the location of the inj token and
    // expr is its argument.
    virtual Node acceptInj(Location loc, Node expr) = 0;

    // Called for "prj" expressions.  loc is the location of the prj token and
    // expr is its argument.
    virtual Node acceptPrj(Location loc, Node expr) = 0;

    virtual Node acceptQualifier(Node qualifierType, Location loc) = 0;

    virtual Node acceptNestedQualifier(Node     qualifier,
                                       Node     qualifierType,
                                       Location loc) = 0;

    virtual Node acceptIfStmt(Location loc, Node condition,
                              Node *consequents, unsigned numConsequents) = 0;

    virtual Node acceptElseStmt(Location loc, Node ifNode,
                                Node *alternates, unsigned numAlternates) = 0;

    virtual Node acceptElsifStmt(Location loc,
                                 Node     ifNode,
                                 Node     condition,
                                 Node    *consequents,
                                 unsigned numConsequents) = 0;

    virtual Node acceptEmptyReturnStmt(Location loc) = 0;

    virtual Node acceptReturnStmt(Location loc, Node retNode) = 0;

    virtual Node acceptAssignmentStmt(Location        loc,
                                      IdentifierInfo *target,
                                      Node            value) = 0;

    // Called when a block statement is about to be parsed.
    virtual Node beginBlockStmt(Location loc, IdentifierInfo *label = 0) = 0;

    // This method is called for each statement associated with the block.
    virtual void acceptBlockStmt(Node block, Node stmt) = 0;

    // Once the last statement of a block has been parsed, this method is called
    // to inform the client that we are leaving the block context established by
    // the last call to beginBlockStmt.
    virtual void endBlockStmt(Node block) = 0;

    // Called when an enumeration type is about to be parsed, supplying the name
    // of the type and its location.  For each literal composing the
    // enumeration, acceptEnumerationLiteral is called with the result of this
    // function.
    virtual Node acceptEnumerationType(IdentifierInfo *name,
                                       Location        loc) = 0;

    // Called for each literal composing an enumeration type, where the first
    // argument is a valid node as returned by acceptEnumerationType.
    virtual void acceptEnumerationLiteral(Node            enumeration,
                                          IdentifierInfo *name,
                                          Location        loc) = 0;
};

} // End comma namespace.

#endif
