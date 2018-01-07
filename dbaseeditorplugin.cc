#include "dbaseeditorplugin.h"
#include "dbaseeditor.h"
#include "dbaseeditorconstants.h"

#include <coreplugin/icore.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/documentmanager.h>
#include <coreplugin/fileiconprovider.h>
#include <coreplugin/id.h>
#include <coreplugin/editormanager/editormanager.h>

#include <extensionsystem/pluginmanager.h>

#include <projectexplorer/applicationlauncher.h>
#include <projectexplorer/kitmanager.h>
#include <projectexplorer/localenvironmentaspect.h>
#include <projectexplorer/runconfiguration.h>
#include <projectexplorer/runconfigurationaspects.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/projectnodes.h>
#include <projectexplorer/runnables.h>
#include <projectexplorer/target.h>

#include <texteditor/texteditorconstants.h>

#include <utils/algorithm.h>
#include <utils/detailswidget.h>
#include <utils/pathchooser.h>
#include <utils/qtcprocess.h>
#include <utils/utilsicons.h>

#include <QtPlugin>

#include <QCoreApplication>
#include <QFormLayout>
#include <QRegExp>

#include <QMessageBox>

using namespace Core;
using namespace ProjectExplorer;
using namespace dBaseEditor::Constants;
using namespace Utils;

namespace dBaseEditor {
namespace Internal {

const char dBaseRunConfigurationPrefix[] = "dBaseEditor.RunConfiguration.";
const char InterpreterKey[] = "dBaseEditor.RunConfiguation.Interpreter";
const char MainScriptKey[] = "dBaseEditor.RunConfiguation.MainScript";
const char dBaseMimeType[] = "text/x-dbase-project"; // ### FIXME
const char dBaseProjectId[] = "dBaseProject";
const char dBaseProjectContext[] = "dBaseProjectContext";

class dBaseRunConfiguration;
class dBaseProjectFile;

static QString scriptFromId(Core::Id id)
{
    return id.suffixAfter(dBaseRunConfigurationPrefix);
}

static Core::Id idFromScript(const QString &target)
{
    return Core::Id(dBaseRunConfigurationPrefix).withSuffix(target);
}

class dBaseProject : public Project
{
public:
    explicit dBaseProject(const Utils::FileName &filename);

    bool addFiles(const QStringList &filePaths);
    bool removeFiles(const QStringList &filePaths);
    bool setFiles(const QStringList &filePaths);
    bool renameFile(const QString &filePath, const QString &newFilePath);
    void refresh();

private:
    RestoreResult fromMap(const QVariantMap &map, QString *errorMessage) override;

    bool saveRawFileList(const QStringList &rawFileList);
    bool saveRawList(const QStringList &rawList, const QString &fileName);
    
        void parseProject();
    QStringList processEntries(const QStringList &paths,
                               QHash<QString, QString> *map = 0) const;

    QStringList m_rawFileList;
    QStringList m_files;
    QHash<QString, QString> m_rawListEntries;
};

class dBaseProjectNode : public ProjectNode
{
public:
    dBaseProjectNode(dBaseProject *project);

    bool showInSimpleTree() const override;
    QString addFileFilter() const override;
    bool renameFile(const QString &filePath, const QString &newFilePath) override;

private:
    dBaseProject *m_project;
};

class dBaseRunConfigurationWidget : public QWidget
{
//    Q_OBJECT
public:
    dBaseRunConfigurationWidget(dBaseRunConfiguration *runConfiguration, QWidget *parent = 0);
    void setInterpreter(const QString &interpreter);

private:
    dBaseRunConfiguration *m_runConfiguration;
    DetailsWidget *m_detailsContainer;
    FancyLineEdit *m_interpreterChooser;
    QLabel *m_scriptLabel;
};

class dBaseRunConfiguration : public RunConfiguration
{
//    Q_OBJECT

    Q_PROPERTY(bool supportsDebugger READ supportsDebugger)
    Q_PROPERTY(QString interpreter READ interpreter)
    Q_PROPERTY(QString mainScript READ mainScript)
    Q_PROPERTY(QString arguments READ arguments)

public:
    explicit dBaseRunConfiguration(Target *target);
    
    QWidget *createConfigurationWidget() override;
    QVariantMap toMap() const override;
    bool fromMap(const QVariantMap &map) override;
    Runnable runnable() const override;

    bool supportsDebugger() const { return true; }
    QString mainScript() const { return m_mainScript; }
    QString arguments() const;
    QString interpreter() const { return m_interpreter; }
    void setInterpreter(const QString &interpreter) { m_interpreter = interpreter; }

private:
    friend class ProjectExplorer::IRunConfigurationFactory;
    void initialize(Core::Id id);

    QString defaultDisplayName() const; // { return QString("dBaseDefaultName"); }

    QString m_interpreter;
    QString m_mainScript;
};

////////////////////////////////////////////////////////////////

dBaseRunConfiguration::dBaseRunConfiguration(Target *target)
    : RunConfiguration(target)
{
    addExtraAspect(new LocalEnvironmentAspect(this, LocalEnvironmentAspect::BaseEnvironmentModifier()));
    addExtraAspect(new ArgumentsAspect(this, "dBaseEditor.RunConfiguration.Arguments"));
    addExtraAspect(new TerminalAspect (this, "dBaseEditor.RunConfiguration.UseTerminal"));
    setDefaultDisplayName(defaultDisplayName());
}

void dBaseRunConfiguration::initialize(Core::Id id)
{
    RunConfiguration::initialize(id);

    m_mainScript = scriptFromId(id);
    setDisplayName(defaultDisplayName());

    Environment sysEnv = Environment::systemEnvironment();  // hier
    const QString exec = sysEnv.searchInPath("dir").toString();
    m_interpreter = exec.isEmpty() ? "dir" : exec;
}

QVariantMap dBaseRunConfiguration::toMap() const
{
    QVariantMap map(RunConfiguration::toMap());
    map.insert(MainScriptKey, m_mainScript);
    map.insert(InterpreterKey, m_interpreter);
    return map;
}

bool dBaseRunConfiguration::fromMap(const QVariantMap &map)
{
    m_mainScript = map.value(MainScriptKey).toString();
    m_interpreter = map.value(InterpreterKey).toString();
    return RunConfiguration::fromMap(map);
}

QString dBaseRunConfiguration::defaultDisplayName() const
{
    return tr("Run %1").arg(m_mainScript);
}

QWidget *dBaseRunConfiguration::createConfigurationWidget()
{
    return new dBaseRunConfigurationWidget(this);
}

Runnable dBaseRunConfiguration::runnable() const
{
    StandardRunnable r;
    QtcProcess::addArg(&r.commandLineArguments, m_mainScript);
    QtcProcess::addArgs(&r.commandLineArguments, extraAspect<ArgumentsAspect>()->arguments());
    r.executable = m_interpreter;
    r.runMode = extraAspect<TerminalAspect>()->runMode();
    r.environment = extraAspect<EnvironmentAspect>()->environment();
    return r;
}

QString dBaseRunConfiguration::arguments() const
{
    auto aspect = extraAspect<ArgumentsAspect>();
    QTC_ASSERT(aspect, return QString());
    return aspect->arguments();
}

dBaseRunConfigurationWidget::dBaseRunConfigurationWidget(dBaseRunConfiguration *runConfiguration, QWidget *parent)
    : QWidget(parent), m_runConfiguration(runConfiguration)
{
    auto fl = new QFormLayout();
    fl->setMargin(0);
    fl->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_interpreterChooser = new FancyLineEdit(this);
    m_interpreterChooser->setText(runConfiguration->interpreter());
    connect(m_interpreterChooser, &QLineEdit::textChanged,
            this, &dBaseRunConfigurationWidget::setInterpreter);

    m_scriptLabel = new QLabel(this);
    m_scriptLabel->setText(runConfiguration->mainScript());

    fl->addRow(tr("Interpreter: "), m_interpreterChooser);
    fl->addRow(tr("Script: "), m_scriptLabel);
    runConfiguration->extraAspect<ArgumentsAspect>()->addToMainConfigurationWidget(this, fl);
    runConfiguration->extraAspect<TerminalAspect>()->addToMainConfigurationWidget(this, fl);

    m_detailsContainer = new DetailsWidget(this);
    m_detailsContainer->setState(DetailsWidget::NoSummary);

    auto details = new QWidget(m_detailsContainer);
    m_detailsContainer->setWidget(details);
    details->setLayout(fl);

    auto vbx = new QVBoxLayout(this);
    vbx->setMargin(0);
    vbx->addWidget(m_detailsContainer);
}

class dBaseRunConfigurationFactory : public IRunConfigurationFactory
{
public:
    dBaseRunConfigurationFactory()
    {
        setObjectName("dBaseRunConfigurationFactory");
    }

    QList<Core::Id> availableCreationIds(Target *parent, CreationMode mode) const override
    {
        Q_UNUSED(mode);
        if (!canHandle(parent))
            return {};
        //return { Core::Id(dBaseExecutableId) };
        
        dBaseProject *project = static_cast<dBaseProject *>(parent->project());
        QList<Core::Id> allIds;
        foreach (const QString &file, project->files(ProjectExplorer::Project::AllFiles))
            allIds.append(idFromScript(file));
        return allIds;
    }

    QString displayNameForId(Core::Id id) const override
    {
        return scriptFromId(id);
    }

    bool canCreate(Target *parent, Core::Id id) const override
    {
       if (!canHandle(parent))
            return false;
        dBaseProject *project = static_cast<dBaseProject *>(parent->project());
        const QString script = scriptFromId(id);
        if (script.endsWith(".dbgprj"))
            return false;
        return project->files(ProjectExplorer::Project::AllFiles).contains(script);
    }

    bool canRestore(Target *parent, const QVariantMap &map) const override
    {
        Q_UNUSED(parent);
        return idFromMap(map).name().startsWith(dBaseRunConfigurationPrefix);
    }

    bool canClone(Target *parent, RunConfiguration *source) const override
    {
        if (!canHandle(parent))
            return false;
        return source->id().name().startsWith(dBaseRunConfigurationPrefix);
    }

    RunConfiguration *clone(Target *parent, RunConfiguration *source) override
    {
        if (!canClone(parent, source))
                    return 0;
        return cloneHelper<dBaseRunConfiguration>(parent, source);
    }

private:
    bool canHandle(Target *parent) const { return dynamic_cast<dBaseProject *>(parent->project()); }

    RunConfiguration *doCreate(Target *parent, Core::Id id) override
    {
        return createHelper<dBaseRunConfiguration>(parent, id);
    }

    RunConfiguration *doRestore(Target *parent, const QVariantMap &map) override
    {
        return createHelper<dBaseRunConfiguration>(parent, idFromMap(map));
    }
};

dBaseProject::dBaseProject(const FileName &fileName) :
    Project(Constants::C_DBASE_MIMETYPE, fileName, [this]() { refresh(); })
{
    setId(dBaseProjectId);
    setProjectContext(Context(dBaseProjectContext));
    setProjectLanguages(Context(ProjectExplorer::Constants::CXX_LANGUAGE_ID)); // hier
    setDisplayName(fileName.toFileInfo().completeBaseName());
    
    QMessageBox::information(0,"wwwww","12221212");
}


static QStringList readLines(const QString &absoluteFileName)
{
    QStringList lines;

    QFile file(absoluteFileName);
    if (file.open(QFile::ReadOnly)) {
        QTextStream stream(&file);

        forever {
            QString line = stream.readLine();
            if (line.isNull())
                break;

            lines.append(line);
        }
    }

    return lines;
}

bool dBaseProject::saveRawFileList(const QStringList &rawFileList)
{
    bool result = saveRawList(rawFileList, projectFilePath().toString());
//    refresh(dBaseProject::Files);
    return result;
}

bool dBaseProject::saveRawList(const QStringList &rawList, const QString &fileName)
{
    FileChangeBlocker changeGuarg(fileName);
    // Make sure we can open the file for writing
    FileSaver saver(fileName, QIODevice::Text);
    if (!saver.hasError()) {
        QTextStream stream(saver.file());
        foreach (const QString &filePath, rawList)
            stream << filePath << '\n';
        saver.setResult(&stream);
    }
    bool result = saver.finalize(ICore::mainWindow());
    return result;
}

bool dBaseProject::addFiles(const QStringList &filePaths)
{
    QStringList newList = m_rawFileList;

    QDir baseDir(projectDirectory().toString());
    foreach (const QString &filePath, filePaths)
        newList.append(baseDir.relativeFilePath(filePath));

    QSet<QString> toAdd;

    foreach (const QString &filePath, filePaths) {
        QString directory = QFileInfo(filePath).absolutePath();
        if (!toAdd.contains(directory))
            toAdd << directory;
    }

    bool result = saveRawList(newList, projectFilePath().toString());
    refresh();

    return result;
}

bool dBaseProject::removeFiles(const QStringList &filePaths)
{
    QStringList newList = m_rawFileList;

    foreach (const QString &filePath, filePaths) {
        QHash<QString, QString>::iterator i = m_rawListEntries.find(filePath);
        if (i != m_rawListEntries.end())
            newList.removeOne(i.value());
    }

    return saveRawFileList(newList);
}

bool dBaseProject::setFiles(const QStringList &filePaths)
{
    QStringList newList;
    QDir baseDir(projectFilePath().toString());
    foreach (const QString &filePath, filePaths)
        newList.append(baseDir.relativeFilePath(filePath));

    return saveRawFileList(newList);
}

bool dBaseProject::renameFile(const QString &filePath, const QString &newFilePath)
{
    QStringList newList = m_rawFileList;

    QHash<QString, QString>::iterator i = m_rawListEntries.find(filePath);
    if (i != m_rawListEntries.end()) {
        int index = newList.indexOf(i.value());
        if (index != -1) {
            QDir baseDir(projectFilePath().toString());
            newList.replace(index, baseDir.relativeFilePath(newFilePath));
        }
    }

    return saveRawFileList(newList);
}

void dBaseProject::parseProject()
{
    m_rawListEntries.clear();
    m_rawFileList = readLines(projectFilePath().toString());
    m_rawFileList << projectFilePath().fileName();
    m_files = processEntries(m_rawFileList, &m_rawListEntries);
}

/**
 * @brief Provides displayName relative to project node
 */
class dBaseFileNode : public FileNode
{
public:
    dBaseFileNode(const Utils::FileName &filePath, const QString &nodeDisplayName,
                   FileType fileType = FileType::Source)
        : FileNode(filePath, fileType, false)
        , m_displayName(nodeDisplayName)
    {}

    QString displayName() const override { return m_displayName; }
private:
    QString m_displayName;
};

void dBaseProject::refresh()
{
    emitParsingStarted();
    parseProject();

    QDir baseDir(projectDirectory().toString());
    auto newRoot = new dBaseProjectNode(this);
    for (const QString &f : m_files) {
        const QString displayName = baseDir.relativeFilePath(f);
        FileType fileType = f.endsWith(".dbgprj") ? FileType::Project : FileType::Source;
        newRoot->addNestedNode(new dBaseFileNode(FileName::fromString(f), displayName, fileType));
    }
    setRootProjectNode(newRoot);

    emitParsingFinished(true);
}

/**
 * Expands environment variables in the given \a string when they are written
 * like $$(VARIABLE).
 */
static void expandEnvironmentVariables(const QProcessEnvironment &env, QString &string)
{
    static QRegExp candidate(QLatin1String("\\$\\$\\((.+)\\)"));

    int index = candidate.indexIn(string);
    while (index != -1) {
        const QString value = env.value(candidate.cap(1));

        string.replace(index, candidate.matchedLength(), value);
        index += value.length();

        index = candidate.indexIn(string, index);
    }
}

/**
 * Expands environment variables and converts the path from relative to the
 * project to an absolute path.
 *
 * The \a map variable is an optional argument that will map the returned
 * absolute paths back to their original \a entries.
 */
QStringList dBaseProject::processEntries(const QStringList &paths,
                                           QHash<QString, QString> *map) const
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QDir projectDir(projectDirectory().toString());

    QFileInfo fileInfo;
    QStringList absolutePaths;
    foreach (const QString &path, paths) {
        QString trimmedPath = path.trimmed();
        if (trimmedPath.isEmpty())
            continue;

        expandEnvironmentVariables(env, trimmedPath);

        trimmedPath = FileName::fromUserInput(trimmedPath).toString();

        fileInfo.setFile(projectDir, trimmedPath);
        if (fileInfo.exists()) {
            const QString absPath = fileInfo.absoluteFilePath();
            absolutePaths.append(absPath);
            if (map)
                map->insert(absPath, trimmedPath);
        }
    }
    absolutePaths.removeDuplicates();
    return absolutePaths;
}

Project::RestoreResult dBaseProject::fromMap(const QVariantMap &map, QString *errorMessage)
{
    Project::RestoreResult res = Project::fromMap(map, errorMessage);
    if (res == RestoreResult::Ok) {
        refresh();

        Kit *defaultKit = KitManager::defaultKit();
        if (!activeTarget() && defaultKit)
            addTarget(createTarget(defaultKit));
    }

    return res;
}

dBaseProjectNode::dBaseProjectNode(dBaseProject *project)
    : ProjectNode(project->projectDirectory())
    , m_project(project)
{
    setDisplayName(project->projectFilePath().toFileInfo().completeBaseName());
}

QHash<QString, QStringList> sortFilesIntoPaths(const QString &base, const QSet<QString> &files)
{
    QHash<QString, QStringList> filesInPath;
    const QDir baseDir(base);

    foreach (const QString &absoluteFileName, files) {
        QFileInfo fileInfo(absoluteFileName);
        FileName absoluteFilePath = FileName::fromString(fileInfo.path());
        QString relativeFilePath;

        if (absoluteFilePath.isChildOf(baseDir)) {
            relativeFilePath = absoluteFilePath.relativeChildPath(FileName::fromString(base)).toString();
        } else {
            // 'file' is not part of the project.
            relativeFilePath = baseDir.relativeFilePath(absoluteFilePath.toString());
            if (relativeFilePath.endsWith('/'))
                relativeFilePath.chop(1);
        }

        filesInPath[relativeFilePath].append(absoluteFileName);
    }
    return filesInPath;
}

bool dBaseProjectNode::showInSimpleTree() const
{
    return true;
}

QString dBaseProjectNode::addFileFilter() const
{
    return QLatin1String("*.dfm");
}

bool dBaseProjectNode::renameFile(const QString &filePath, const QString &newFilePath)
{
    return m_project->renameFile(filePath, newFilePath);
}

// dBaseRunConfigurationWidget

void dBaseRunConfigurationWidget::setInterpreter(const QString &interpreter)
{
    m_runConfiguration->setInterpreter(interpreter);
}

////////////////////////////////////////////////////////////////////////////////////
//
// dBaseEditorPlugin
//
////////////////////////////////////////////////////////////////////////////////////

static dBaseEditorPlugin *m_instance = 0;

dBaseEditorPlugin::dBaseEditorPlugin()
{
    m_instance = this;
}

dBaseEditorPlugin::~dBaseEditorPlugin()
{
    m_instance = 0;
}

bool dBaseEditorPlugin::initialize(const QStringList &arguments, QString *errorMessage)
{
    Q_UNUSED(arguments)
    Q_UNUSED(errorMessage)

    ProjectManager::registerProjectType<dBaseProject>(dBaseMimeType);

    addAutoReleasedObject(new dBaseEditorFactory);
    addAutoReleasedObject(new dBaseRunConfigurationFactory);

    auto constraint = [](RunConfiguration *runConfiguration) {
        auto rc = dynamic_cast<dBaseRunConfiguration *>(runConfiguration);
        return  rc && !rc->interpreter().isEmpty();
    };
    RunControl::registerWorker<SimpleTargetRunner>(ProjectExplorer::Constants::NORMAL_RUN_MODE, constraint);

    return true;
}

void dBaseEditorPlugin::extensionsInitialized()
{
    // Initialize editor actions handler
    // Add MIME overlay icons (these icons displayed at Project dock panel)
    const QIcon icon = QIcon::fromTheme(C_DBASE_MIME_ICON);
    if (!icon.isNull())
        Core::FileIconProvider::registerIconOverlayForMimeType(icon, C_DBASE_MIMETYPE);
}

} // namespace Internal
} // namespace dBaseEditor

