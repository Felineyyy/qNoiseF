#ifndef QNOISEF_H
#define QNOISEF_H

#include <ccStdPluginInterface.h>
#include <QObject>

class qNoiseF : public QObject, public ccStdPluginInterface
{
    Q_OBJECT
    Q_INTERFACES(ccPluginInterface ccStdPluginInterface)
    Q_PLUGIN_METADATA(IID "cccorp.cloudcompare.plugin.qNoiseF" FILE "../info.json")

public:
    explicit qNoiseF(QObject* parent = nullptr);

    virtual QString getName() const override { return "Noise Filter (qNoiseF)"; }
    virtual QString getDescription() const override { return "Noise filter with label exclusion"; }
    virtual QIcon getIcon() const override;

    virtual void onNewSelection(const ccHObject::Container& selectedEntities) override;
    QList<QAction *> getActions() override;
protected slots:
    void doAction();
    void doNoiseFilter(bool useRadius, double radius, int knn,
                       int minNeighbors, double maxError,
                       bool selectIsolated, int excludedLabel);

private:
    ccPointCloud* m_cloud;
    QAction* m_action;
};

#endif // QNOISEF_H