#include "dbaseeditor.h"
#include "dbaseeditorconstants.h"

#include <texteditor/texteditoractionhandler.h>
#include <texteditor/texteditorconstants.h>
#include <texteditor/textdocument.h>

#include <utils/qtcassert.h>

#include <QCoreApplication>

using namespace TextEditor;

namespace dBaseEditor {
namespace Internal {

dBaseEditorFactory::dBaseEditorFactory()
{
    setId(Constants::C_DBASEEDITOR_ID);
    setDisplayName(
        QCoreApplication::translate(
        "OpenWith::Editors",
        Constants::C_EDITOR_DISPLAY_NAME));
    addMimeType(Constants::C_DBASE_MIMETYPE);
    
    setEditorActionHandlers(
             TextEditorActionHandler::Format
           | TextEditorActionHandler::UnCommentSelection
           | TextEditorActionHandler::UnCollapseAll);
           
    setDocumentCreator([] { return new TextDocument(Constants::C_DBASEEDITOR_ID); });
    setCommentDefinition(Utils::CommentDefinition::HashStyle);
    setParenthesesMatchingEnabled(true);
    setMarksVisible(true);
    setCodeFoldingSupported(true);
}

}  // namespace: Internal
}  // namespace: dBaseEditor
