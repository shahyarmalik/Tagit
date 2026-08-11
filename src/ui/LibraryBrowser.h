#ifndef TAGIT_LIBRARY_BROWSER_H
#define TAGIT_LIBRARY_BROWSER_H

#include <QWidget>
#include <memory>

class QTreeView;
class QFileSystemModel;

namespace tagit {

class IPlatformServices;

/**
 * @brief Folder-tree browser for navigating the local music library.
 *
 * Uses a QFileSystemModel rooted at the user's home directory so the user can
 * navigate to their music folders and select folders to scan/import.
 */
class LibraryBrowser : public QWidget {
    Q_OBJECT
public:
    explicit LibraryBrowser(std::shared_ptr<IPlatformServices> platform,
                            QWidget *parent = nullptr);

signals:
    void folderSelected(const QString &path);

private:
    QTreeView *m_tree = nullptr;
    QFileSystemModel *m_fsModel = nullptr;
    std::shared_ptr<IPlatformServices> m_platform;
};

} // namespace tagit

#endif // TAGIT_LIBRARY_BROWSER_H

