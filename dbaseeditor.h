#pragma once

#include <texteditor/texteditor.h>

namespace dBaseEditor {
namespace Internal {

class dBaseEditorFactory: public TextEditor::TextEditorFactory
{
public:
    dBaseEditorFactory();
};

}  // namespace: Internal
}  // namespace: dBaseEditor
