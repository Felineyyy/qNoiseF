#ifndef NOISEFDIALOG_H
#define NOISEFDIALOG_H

#include <QDialog>

class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QRadioButton;
class QButtonGroup;

class NoiseFDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NoiseFDialog(QWidget* parent = nullptr);

    bool isRadiusMode() const;
    double getRadius() const;
    int getKNN() const;
    int getMinNeighbors() const;
    double getMaxError() const;
    bool selectIsolated() const;
    int getExcludedLabel() const;

private slots:
    void onModeChanged();

private:
    QButtonGroup* m_modeGroup;
    QRadioButton* m_radiusRadio;
    QRadioButton* m_knnRadio;
    QDoubleSpinBox* m_radiusSpin;
    QSpinBox* m_knnSpin;
    QSpinBox* m_minNeighborsSpin;
    QDoubleSpinBox* m_maxErrorSpin;
    QCheckBox* m_isolatedCheck;
    QSpinBox* m_excludedLabelSpin;
};

#endif // NOISEFDIALOG_H
