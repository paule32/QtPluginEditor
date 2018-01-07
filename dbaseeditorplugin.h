#pragma once

#include <extensionsystem/iplugin.h>

namespace dBaseEditor {
namespace Internal {

class dBaseEditorPlugin: public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "dbaseeditor.json")
    
public:
     dBaseEditorPlugin();
    ~dBaseEditorPlugin() override;
    
    bool initialize(const QStringList &arguments, QString *errorMessage) override;
    void extensionsInitialized() override;
};

}  // namespace: Internal
}  // namespace::dBaseEditor
