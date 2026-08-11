#include "ArtworkInspector.h"
#include "../platform/TagService.h"
#include "../core/Logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QPixmap>
#include <QImage>
#include <QImageReader>
#include <QMessageBox>
#include <QFrame>
#include <QBuffer>

namespace tagit {

ArtworkInspector::ArtworkInspector(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    clear();
}

void ArtworkInspector::setTagService(TagService *tagService)
{
    m_tagService = tagService;
}

void ArtworkInspector::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    // File name header
    m_fileLabel = new QLabel(tr("No file selected"), this);
    m_fileLabel->setWordWrap(true);
    m_fileLabel->setStyleSheet("color: #a6adc8; font-size: 11px;");
    layout->addWidget(m_fileLabel);

    // Artwork Container Box (Card)
    auto *previewCard = new QFrame(this);
    previewCard->setStyleSheet(
        "QFrame { background: #181825; border: 1px solid #313244; border-radius: 8px; }");
    auto *cardLayout = new QVBoxLayout(previewCard);
    cardLayout->setContentsMargins(12, 12, 12, 12);
    cardLayout->setAlignment(Qt::AlignCenter);

    m_imagePreview = new QLabel(previewCard);
    m_imagePreview->setFixedSize(220, 220);
    m_imagePreview->setAlignment(Qt::AlignCenter);
    m_imagePreview->setStyleSheet(
        "QLabel { background: #11111b; border: 1px dashed #45475a; border-radius: 6px; color: #6c7086; }");
    cardLayout->addWidget(m_imagePreview);

    m_infoLabel = new QLabel(previewCard);
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setStyleSheet("color: #bac2de; font-size: 11px; margin-top: 6px;");
    cardLayout->addWidget(m_infoLabel);

    layout->addWidget(previewCard);

    // Button Row for Change/Add, Remove, Export
    auto *btnLayout = new QVBoxLayout();
    btnLayout->setSpacing(6);

    m_changeButton = new QPushButton(tr("🖼️  Change / Add Artwork…"), this);
    m_changeButton->setStyleSheet(
        "QPushButton { background: #89b4fa; color: #11111b; "
        "border-radius: 4px; padding: 6px 14px; font-weight: bold; }"
        "QPushButton:hover   { background: #b4befe; }"
        "QPushButton:pressed { background: #74c7ec; }"
        "QPushButton:disabled { background: #45475a; color: #6c7086; }");

    auto *secondaryRow = new QHBoxLayout();
    secondaryRow->setSpacing(6);

    m_removeButton = new QPushButton(tr("🗑️ Remove"), this);
    m_removeButton->setStyleSheet(
        "QPushButton { background: #313244; color: #f38ba8; "
        "border: 1px solid #45475a; border-radius: 4px; padding: 5px 10px; }"
        "QPushButton:hover   { background: #45475a; color: #eba0ac; }"
        "QPushButton:pressed { background: #585b70; }"
        "QPushButton:disabled { color: #585b70; border-color: #313244; }");

    m_exportButton = new QPushButton(tr("💾 Export…"), this);
    m_exportButton->setStyleSheet(
        "QPushButton { background: #313244; color: #cdd6f4; "
        "border: 1px solid #45475a; border-radius: 4px; padding: 5px 10px; }"
        "QPushButton:hover   { background: #45475a; }"
        "QPushButton:pressed { background: #585b70; }"
        "QPushButton:disabled { color: #585b70; border-color: #313244; }");

    secondaryRow->addWidget(m_removeButton);
    secondaryRow->addWidget(m_exportButton);

    btnLayout->addWidget(m_changeButton);
    btnLayout->addLayout(secondaryRow);

    layout->addLayout(btnLayout);

    // Status label
    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet("color: #a6adc8; font-size: 11px;");
    layout->addWidget(m_statusLabel);

    layout->addStretch();

    // Connections
    connect(m_changeButton, &QPushButton::clicked, this, &ArtworkInspector::onChangeArtworkClicked);
    connect(m_removeButton, &QPushButton::clicked, this, &ArtworkInspector::onRemoveArtworkClicked);
    connect(m_exportButton, &QPushButton::clicked, this, &ArtworkInspector::onExportArtworkClicked);
}

void ArtworkInspector::setSong(const QString &filePath, const AudioMetadata &metadata)
{
    m_filePath = filePath;
    m_metadata = metadata;
    m_statusLabel->clear();

    const QString fileName = QFileInfo(filePath).fileName();
    m_fileLabel->setText(fileName.isEmpty() ? filePath : fileName);
    m_fileLabel->setToolTip(filePath);

    // Extract artwork data on demand from file if TagService is available
    if (m_tagService && !m_filePath.isEmpty()) {
        QString mime;
        m_currentArtworkData = m_tagService->extractArtwork(m_filePath, &mime);
        m_currentMimeType = mime;
    } else {
        m_currentArtworkData = metadata.artworkData;
        m_currentMimeType.clear();
    }

    updatePreview();
    setControlsEnabled(!m_filePath.isEmpty());
}

void ArtworkInspector::clear()
{
    m_filePath.clear();
    m_metadata = AudioMetadata();
    m_currentArtworkData.clear();
    m_currentMimeType.clear();

    m_fileLabel->setText(tr("No file selected"));
    m_fileLabel->setToolTip(QString());
    m_statusLabel->clear();

    updatePreview();
    setControlsEnabled(false);
}

void ArtworkInspector::updatePreview()
{
    if (m_currentArtworkData.isEmpty()) {
        m_imagePreview->setPixmap(QPixmap());
        m_imagePreview->setText(tr("No Artwork\nEmbedded"));
        m_infoLabel->setText(tr("No image"));
        m_removeButton->setEnabled(false);
        m_exportButton->setEnabled(false);
        return;
    }

    QPixmap pix;
    if (pix.loadFromData(m_currentArtworkData)) {
        QPixmap scaled = pix.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_imagePreview->setText(QString());
        m_imagePreview->setPixmap(scaled);

        const double kb = static_cast<double>(m_currentArtworkData.size()) / 1024.0;
        QString format = m_currentMimeType.isEmpty() ? tr("Image") : m_currentMimeType;
        if (format.contains("jpeg", Qt::CaseInsensitive)) format = "JPEG";
        else if (format.contains("png", Qt::CaseInsensitive)) format = "PNG";

        m_infoLabel->setText(QString("%1 × %2 px • %3 • %4 KB")
                                 .arg(pix.width())
                                 .arg(pix.height())
                                 .arg(format)
                                 .arg(kb, 0, 'f', 1));
        m_removeButton->setEnabled(true);
        m_exportButton->setEnabled(true);
    } else {
        m_imagePreview->setPixmap(QPixmap());
        m_imagePreview->setText(tr("Invalid Image\nFormat"));
        m_infoLabel->setText(tr("Corrupted or unsupported format"));
        m_removeButton->setEnabled(true);
        m_exportButton->setEnabled(false);
    }
}

void ArtworkInspector::setControlsEnabled(bool enabled)
{
    m_changeButton->setEnabled(enabled);
    m_removeButton->setEnabled(enabled && !m_currentArtworkData.isEmpty());
    m_exportButton->setEnabled(enabled && !m_currentArtworkData.isEmpty());
}

void ArtworkInspector::onChangeArtworkClicked()
{
    if (m_filePath.isEmpty()) return;

    const QString selectedFile = QFileDialog::getOpenFileName(
        this,
        tr("Select Artwork Image"),
        QString(),
        tr("Image Files (*.jpg *.jpeg *.png *.webp *.bmp);;All Files (*)"));

    if (selectedFile.isEmpty()) return;

    QFile imgFile(selectedFile);
    if (!imgFile.open(QIODevice::ReadOnly)) {
        m_statusLabel->setStyleSheet("color: #f38ba8;");
        m_statusLabel->setText(tr("Failed to read selected image file."));
        return;
    }

    QByteArray bytes = imgFile.readAll();
    QPixmap testPix;
    if (!testPix.loadFromData(bytes)) {
        m_statusLabel->setStyleSheet("color: #f38ba8;");
        m_statusLabel->setText(tr("Unsupported or invalid image format."));
        return;
    }

    // Determine mime type
    QString mime = "image/jpeg";
    const QString ext = QFileInfo(selectedFile).suffix().toLower();
    if (ext == "png") {
        mime = "image/png";
    }

    if (m_tagService) {
        bool ok = m_tagService->writeArtwork(m_filePath, bytes, mime);
        if (ok) {
            m_currentArtworkData = bytes;
            m_currentMimeType = mime;
            updatePreview();
            m_statusLabel->setStyleSheet("color: #a6e3a1;");
            m_statusLabel->setText(tr("✓ Artwork updated successfully."));
            emit artworkChanged(m_filePath, true);
        } else {
            m_statusLabel->setStyleSheet("color: #f38ba8;");
            m_statusLabel->setText(tr("Failed to embed artwork into audio file."));
        }
    }
}

void ArtworkInspector::onRemoveArtworkClicked()
{
    if (m_filePath.isEmpty()) return;

    auto reply = QMessageBox::question(
        this,
        tr("Remove Artwork"),
        tr("Are you sure you want to remove the embedded artwork from this song?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    if (m_tagService) {
        bool ok = m_tagService->removeArtwork(m_filePath);
        if (ok) {
            m_currentArtworkData.clear();
            m_currentMimeType.clear();
            updatePreview();
            m_statusLabel->setStyleSheet("color: #a6e3a1;");
            m_statusLabel->setText(tr("✓ Artwork removed."));
            emit artworkChanged(m_filePath, false);
        } else {
            m_statusLabel->setStyleSheet("color: #f38ba8;");
            m_statusLabel->setText(tr("Failed to remove artwork from file."));
        }
    }
}

void ArtworkInspector::onExportArtworkClicked()
{
    if (m_filePath.isEmpty() || m_currentArtworkData.isEmpty()) return;

    QString defaultExt = m_currentMimeType.contains("png", Qt::CaseInsensitive) ? "png" : "jpg";
    QString filter = (defaultExt == "png")
                         ? tr("PNG Image (*.png);;JPEG Image (*.jpg);;All Files (*)")
                         : tr("JPEG Image (*.jpg);;PNG Image (*.png);;All Files (*)");

    const QString targetFile = QFileDialog::getSaveFileName(
        this,
        tr("Export Artwork"),
        "cover." + defaultExt,
        filter);

    if (targetFile.isEmpty()) return;

    QFile out(targetFile);
    if (out.open(QIODevice::WriteOnly)) {
        out.write(m_currentArtworkData);
        out.close();
        m_statusLabel->setStyleSheet("color: #a6e3a1;");
        m_statusLabel->setText(tr("✓ Artwork exported: %1").arg(QFileInfo(targetFile).fileName()));
    } else {
        m_statusLabel->setStyleSheet("color: #f38ba8;");
        m_statusLabel->setText(tr("Failed to export artwork file."));
    }
}

} // namespace tagit
