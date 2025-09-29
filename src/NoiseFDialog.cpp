#include "NoiseFDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QPushButton>

NoiseFDialog::NoiseFDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Noise Filter Settings");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* modeLayout = new QHBoxLayout();
    m_radiusRadio = new QRadioButton("Use Radius");
    m_knnRadio = new QRadioButton("Use KNN");
    modeLayout->addWidget(m_radiusRadio);
    modeLayout->addWidget(m_knnRadio);
    m_radiusRadio->setChecked(true);

    mainLayout->addLayout(modeLayout);

    QHBoxLayout* radiusLayout = new QHBoxLayout();
    radiusLayout->addWidget(new QLabel("Neighborhood Radius:"));
    m_radiusSpin = new QDoubleSpinBox();
    m_radiusSpin->setRange(0.0001, 1000.0);
    m_radiusSpin->setDecimals(4);
    m_radiusSpin->setValue(0.5);
    radiusLayout->addWidget(m_radiusSpin);

    mainLayout->addLayout(radiusLayout);

    QHBoxLayout* knnLayout = new QHBoxLayout();
    knnLayout->addWidget(new QLabel("KNN:"));
    m_knnSpin = new QSpinBox();
    m_knnSpin->setRange(1, 100);
    m_knnSpin->setValue(6);
    knnLayout->addWidget(m_knnSpin);

    mainLayout->addLayout(knnLayout);

    QHBoxLayout* minNeighborsLayout = new QHBoxLayout();
    minNeighborsLayout->addWidget(new QLabel("Min Neighbors:"));
    m_minNeighborsSpin = new QSpinBox();
    m_minNeighborsSpin->setRange(1, 100);
    m_minNeighborsSpin->setValue(6);
    minNeighborsLayout->addWidget(m_minNeighborsSpin);

    mainLayout->addLayout(minNeighborsLayout);

    QHBoxLayout* maxErrorLayout = new QHBoxLayout();
    maxErrorLayout->addWidget(new QLabel("Max Error:"));
    m_maxErrorSpin = new QDoubleSpinBox();
    m_maxErrorSpin->setRange(0.0, 1000.0);
    m_maxErrorSpin->setDecimals(4);
    m_maxErrorSpin->setValue(0.0);
    maxErrorLayout->addWidget(m_maxErrorSpin);

    mainLayout->addLayout(maxErrorLayout);

    QHBoxLayout* isolatedLayout = new QHBoxLayout();
    m_isolatedCheck = new QCheckBox("Select Isolated Points");
    isolatedLayout->addWidget(m_isolatedCheck);

    mainLayout->addLayout(isolatedLayout);

    QHBoxLayout* excludedLabelLayout = new QHBoxLayout();
    excludedLabelLayout->addWidget(new QLabel("Excluded Label:"));
    m_excludedLabelSpin = new QSpinBox();
    m_excludedLabelSpin->setRange(-100000, 100000);
    m_excludedLabelSpin->setValue(1); // ground by default
    excludedLabelLayout->addWidget(m_excludedLabelSpin);

    mainLayout->addLayout(excludedLabelLayout);

    QHBoxLayout* buttonsLayout = new QHBoxLayout();
    QPushButton* okBtn = new QPushButton("OK");
    QPushButton* cancelBtn = new QPushButton("Cancel");
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    buttonsLayout->addWidget(okBtn);
    buttonsLayout->addWidget(cancelBtn);

    mainLayout->addLayout(buttonsLayout);
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