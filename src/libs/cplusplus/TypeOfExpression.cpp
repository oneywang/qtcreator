/***************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2008 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact:  Qt Software Information (qt-info@nokia.com)
**
**
** Non-Open Source Usage
**
** Licensees may use this file in accordance with the Qt Beta Version
** License Agreement, Agreement version 2.2 provided with the Software or,
** alternatively, in accordance with the terms contained in a written
** agreement between you and Nokia.
**
** GNU General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU General
** Public License versions 2.0 or 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the packaging
** of this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
**
** http://www.fsf.org/licensing/licenses/info/GPLv2.html and
** http://www.gnu.org/copyleft/gpl.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt GPL Exception
** version 1.2, included in the file GPL_EXCEPTION.txt in this package.
**
***************************************************************************/

#include "TypeOfExpression.h"

#include <AST.h>
#include <TranslationUnit.h>
#include <cplusplus/LookupContext.h>
#include <cplusplus/ResolveExpression.h>
#include <cplusplus/pp.h>

using namespace CPlusPlus;

TypeOfExpression::TypeOfExpression():
    m_ast(0)
{
}

void TypeOfExpression::setDocuments(const QMap<QString, Document::Ptr> &documents)
{
    m_documents = documents;
    m_lookupContext = LookupContext();
}

QList<TypeOfExpression::Result> TypeOfExpression::operator()(const QString &expression,
                                                             Document::Ptr document,
                                                             Symbol *lastVisibleSymbol,
                                                             PreprocessMode mode)
{
    QString code = expression;
    if (mode == Preprocess)
        code = preprocessedExpression(expression, m_documents, document);
    Document::Ptr expressionDoc = documentForExpression(code);
    m_ast = extractExpressionAST(expressionDoc);

    m_lookupContext = LookupContext(lastVisibleSymbol, expressionDoc,
                                    document, m_documents);

    ResolveExpression resolveExpression(m_lookupContext);
    return resolveExpression(m_ast);
}

ExpressionAST *TypeOfExpression::ast() const
{
    return m_ast;
}

const LookupContext &TypeOfExpression::lookupContext() const
{
    return m_lookupContext;
}

ExpressionAST *TypeOfExpression::expressionAST() const
{
    return extractExpressionAST(m_lookupContext.expressionDocument());
}

ExpressionAST *TypeOfExpression::extractExpressionAST(Document::Ptr doc) const
{
    if (! doc->translationUnit()->ast())
        return 0;

    return doc->translationUnit()->ast()->asExpression();
}

Document::Ptr TypeOfExpression::documentForExpression(const QString &expression) const
{
    // create the expression's AST.
    Document::Ptr doc = Document::create(QLatin1String("<completion>"));
    const QByteArray bytes = expression.toUtf8();
    doc->setSource(bytes);
    doc->parse(Document::ParseExpression);
    return doc;
}

void TypeOfExpression::processEnvironment(QMap<QString, Document::Ptr> documents,
                                          Document::Ptr doc, Environment *env,
                                          QSet<QString> *processed) const
{
    if (! doc)
        return;
    if (processed->contains(doc->fileName()))
        return;
    processed->insert(doc->fileName());
    foreach (const Document::Include &incl, doc->includes()) {
        processEnvironment(documents,
                           documents.value(incl.fileName()),
                           env, processed);
    }
    foreach (const Macro &macro, doc->definedMacros())
        env->bind(macro);
}

QString TypeOfExpression::preprocessedExpression(const QString &expression,
                                                 QMap<QString, Document::Ptr> documents,
                                                 Document::Ptr thisDocument) const
{
    Environment env;
    QSet<QString> processed;
    processEnvironment(documents, thisDocument,
                       &env, &processed);
    const QByteArray code = expression.toUtf8();
    pp preproc(0, env);
    QByteArray preprocessedCode;
    preproc("<expression>", code, &preprocessedCode);
    return QString::fromUtf8(preprocessedCode);
}
