#include "NoiseFDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QPushButton>
#include <QButtonGroup>
#include <QDialogButtonBox>

NoiseFDialog::NoiseFDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Noise Filter with Label Exclusion");
    setMinimumWidth(450);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // ===== Neighborhood Mode Group =====
    QGroupBox* modeGroup = new QGroupBox("Neighborhood Mode");
    QVBoxLayout* modeLayout = new QVBoxLayout(modeGroup);
    
    m_modeGroup = new QButtonGroup(this);
    m_radiusRadio = new QRadioButton("Sphere Radius");
    m_knnRadio = new QRadioButton("K Nearest Neighbors (KNN)");
    m_modeGroup->addButton(m_radiusRadio, 0);
    m_modeGroup->addButton(m_knnRadio, 1);
    
    modeLayout->addWidget(m_radiusRadio);
    modeLayout->addWidget(m_knnRadio);
    
    m_radiusRadio->setChecked(true);
    mainLayout->addWidget(modeGroup);

    // ===== Parameters Group =====
    QGroupBox* paramGroup = new QGroupBox("Neighborhood Parameters");
    QGridLayout* paramLayout = new QGridLayout(paramGroup);
    
    int row = 0;
    
    // Radius parameter
    paramLayout->addWidget(new QLabel("Sphere Radius:"), row, 0);
    m_radiusSpin = new QDoubleSpinBox();
    m_radiusSpin->setRange(0.0001, 10000.0);
    m_radiusSpin->setDecimals(4);
    m_radiusSpin->setValue(0.1);
    m_radiusSpin->setToolTip("Radius for neighbor search (should capture at least 6 points typically)");
    paramLayout->addWidget(m_radiusSpin, row, 1);
    row++;
    
    // KNN parameter
    paramLayout->addWidget(new QLabel("K Neighbors:"), row, 0);
    m_knnSpin = new QSpinBox();
    m_knnSpin->setRange(3, 1000);
    m_knnSpin->setValue(6);
    m_knnSpin->setToolTip("Number of nearest neighbors (only suitable for clouds with constant density)");
    paramLayout->addWidget(m_knnSpin, row, 1);
    row++;
    
    mainLayout->addWidget(paramGroup);

    // ===== Filtering Criteria Group =====
    QGroupBox* filterGroup = new QGroupBox("Filtering Criteria");
    QGridLayout* filterLayout = new QGridLayout(filterGroup);
    
    row = 0;
    
    // Max error (absolute distance to fitted plane)
    filterLayout->addWidget(new QLabel("Max Error (absolute):"), row, 0);
    m_maxErrorSpin = new QDoubleSpinBox();
    m_maxErrorSpin->setRange(0.0, 10000.0);
    m_maxErrorSpin->setDecimals(4);
    m_maxErrorSpin->setValue(0.01);
    m_maxErrorSpin->setToolTip("Maximum distance from point to fitted plane (points beyond this are removed)");
    filterLayout->addWidget(m_maxErrorSpin, row, 1);
    row++;
    
    // Remove isolated points checkbox
    m_isolatedCheck = new QCheckBox("Remove isolated points");
    m_isolatedCheck->setChecked(false);
    m_isolatedCheck->setToolTip("Remove points with fewer neighbors than the threshold below");
    filterLayout->addWidget(m_isolatedCheck, row, 0, 1, 2);
    row++;
    
    // Min neighbors threshold (for isolated point removal)
    filterLayout->addWidget(new QLabel("Min Neighbors:"), row, 0);
    m_minNeighborsSpin = new QSpinBox();
    m_minNeighborsSpin->setRange(1, 100);
    m_minNeighborsSpin->setValue(3);
    m_minNeighborsSpin->setToolTip("Minimum number of neighbors required (used only if 'Remove isolated points' is checked)");
    m_minNeighborsSpin->setEnabled(false);
    paramLayout->addWidget(m_minNeighborsSpin, row, 1);
    row++;
    
    mainLayout->addWidget(filterGroup);

    // ===== Label Exclusion Group =====
    QGroupBox* labelGroup = new QGroupBox("Label Exclusion (NEW FEATURE)");
    QGridLayout* labelLayout = new QGridLayout(labelGroup);
    
    labelLayout->addWidget(new QLabel("Excluded Label:"), 0, 0);
    m_excludedLabelSpin = new QSpinBox();
    m_excludedLabelSpin->setRange(-100000, 100000);
    m_excludedLabelSpin->setValue(1);
    m_excludedLabelSpin->setToolTip("Points with this label will NOT be counted as neighbors\n(Default: 1 = ground points)");
    labelLayout->addWidget(m_excludedLabelSpin, 0, 1);
    
    QLabel* noteLabel = new QLabel("<i>Note: Points with this label will be excluded from neighbor\ncalculations but will still be processed and filtered.</i>");
    noteLabel->setWordWrap(true);
    labelLayout->addWidget(noteLabel, 1, 0, 1, 2);
    
    mainLayout->addWidget(labelGroup);

    // ===== Buttons =====
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    mainLayout->addWidget(buttonBox);

    // Connect signals
    connect(m_radiusRadio, &QRadioButton::toggled, this, &NoiseFDialog::onModeChanged);
    connect(m_isolatedCheck, &QCheckBox::toggled, m_minNeighborsSpin, &QSpinBox::setEnabled);
    
    // Initialize UI state
    onModeChanged();
}

void NoiseFDialog::onModeChanged()
{
    bool radiusMode = m_radiusRadio->isChecked();
    m_radiusSpin->setEnabled(radiusMode);
    m_knnSpin->setEnabled(!radiusMode);
}

bool NoiseFDialog::isRadiusMode() const
{
    return m_radiusRadio->isChecked();
}

double NoiseFDialog::getRadius() const
{
    return m_radiusSpin->value();
}

int NoiseFDialog::getKNN() const
{
    return m_knnSpin->value();
}

int NoiseFDialog::getMinNeighbors() const
{
    return m_minNeighborsSpin->value();
}

double NoiseFDialog::getMaxError() const
{
    return m_maxErrorSpin->value();
}

bool NoiseFDialog::selectIsolated() const
{
    return m_isolatedCheck->isChecked();
}

int NoiseFDialog::getExcludedLabel() const
{
    return m_excludedLabelSpin->value();
}
