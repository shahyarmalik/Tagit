#include "LibraryBrowser.h"
#include "../platform/IPlatformServices.h"

#include <QTreeView>
#include <QFileSystemModel>
#include <QVBoxLayout>
#include <QDir>
#include <utility>

namespace tagit {

LibraryBrowser::LibraryBrowser(std::shared_ptr<IPlatformServices> platform, QWidget *parent)
    : QWidget(parent)
    , m_platform(std::move(platform))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tree = new QTreeView(this);
    m_tree->setHeaderHidden(true);
    m_tree->setAnimated(true);
    m_tree->setIndentation(20);

    m_fsModel = new QFileSystemModel(this);
    m_fsModel->setRootPath(QDir::homePath());
    m_fsModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Drives);

    m_tree->setModel(m_fsModel);
    m_tree->setRootIndex(m_fsModel->index(QDir::homePath()));

    layout->addWidget(m_tree);

    connect(m_tree, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
        const QString path = m_fsModel->filePath(index);
        if (m_fsModel->isDir(index)) {
            emit folderSelected(path);
        }
    });
}

} // namespace tagit

