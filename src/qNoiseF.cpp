#include "qNoiseF.h"
#include "NoiseFDialog.h"

#include <ccPointCloud.h>
#include <ccScalarField.h>
#include <ccProgressDialog.h>
#include <ccLog.h>
#include <QIcon>
#include <vector>

qNoiseF::qNoiseF(QObject* parent)
    : QObject(parent)
    , ccStdPluginInterface(":/CC/plugin/qNoiseF/info.json" )
    , m_cloud(nullptr)
{
}

QList<QAction *> qNoiseF::getActions()
{
    if ( !m_action )
    {
        m_action = new QAction( getName(), this );
        m_action->setToolTip( getDescription() );
        m_action->setIcon( getIcon() );
        connect( m_action, &QAction::triggered, this, &qNoiseF::doAction );
    }
    return { m_action };
}

QIcon qNoiseF::getIcon() const
{
    return QIcon(":/CC/plugin/qNoiseF/icon.png");
}

void qNoiseF::onNewSelection(const ccHObject::Container& selectedEntities)
{
    m_cloud = nullptr;
    for (ccHObject* entity : selectedEntities)
    {
        m_cloud = ccHObjectCaster::ToPointCloud(entity);
        if (m_cloud)
            break;
    }
    if (!m_cloud)
    {
        ccLog::Warning("[qNoiseF] Select a point cloud!");
        return;
    }

    NoiseFDialog dlg;
    if (dlg.exec() != QDialog::Accepted)
        return;

    doNoiseFilter(
        dlg.isRadiusMode(),
        dlg.getRadius(),
        dlg.getKNN(),
        dlg.getMinNeighbors(),
        dlg.getMaxError(),
        dlg.selectIsolated(),
        dlg.getExcludedLabel()
    );
}

void qNoiseF::doNoiseFilter(bool useRadius, double radius, int knn,
                            int minNeighbors, double maxError,
                            bool selectIsolated, int excludedLabel)
{
    if (!m_cloud)
        return;

    int labelSfIdx = m_cloud->getScalarFieldIndexByName("Label");
    ccScalarField* labelSf = nullptr;
    if (labelSfIdx >= 0)
        labelSf = static_cast<ccScalarField*>(m_cloud->getScalarField(labelSfIdx));

    // Build a list of candidate point indices (not excluded)
    std::vector<unsigned> candidateIndices;
    for (unsigned i = 0; i < m_cloud->size(); ++i)
    {
        if (!labelSf || labelSf->getValue(i) != excludedLabel)
            candidateIndices.push_back(i);
    }

    std::vector<bool> keep(m_cloud->size(), true);

    // KNN or radius-based neighborhood search
    ccProgressDialog pDlg(false);
    pDlg.setMethodTitle("Noise Filter");
    pDlg.setInfo("Filtering points...");
    pDlg.setRange(0, static_cast<int>(candidateIndices.size()));

    for (size_t idx = 0; idx < candidateIndices.size(); ++idx)
    {
        if (pDlg.isCanceled())
            break;
        unsigned ptIdx = candidateIndices[idx];
        const CCVector3* P = m_cloud->getPoint(ptIdx);

        unsigned neighborCount = 0;
        double localError = 0.0;

        // Neighborhood search, excluding points with excluded label
        if (useRadius) {
            // Radius mode
            for (size_t j = 0; j < m_cloud->size(); ++j)
            {
                if (j == ptIdx) continue;
                if (labelSf && labelSf->getValue(j) == excludedLabel)
                    continue;
                const CCVector3* Q = m_cloud->getPoint(j);
                if (P->squareDist(*Q) <= radius * radius)
                {
                    ++neighborCount;
                    // (Optional: accumulate error, if needed)
                }
            }
        } else {
            // KNN mode
            std::vector<std::pair<double, unsigned>> dists; // (distance, index)
            for (size_t j = 0; j < m_cloud->size(); ++j)
            {
                if (j == ptIdx) continue;
                if (labelSf && labelSf->getValue(j) == excludedLabel)
                    continue;
                const CCVector3* Q = m_cloud->getPoint(j);
                double sqDist = P->squareDist(*Q);
                dists.emplace_back(sqDist, j);
            }
            std::sort(dists.begin(), dists.end());
            neighborCount = std::min(knn, static_cast<int>(dists.size()));
            // (Optional: accumulate error, if needed)
        }

        // Apply minNeighbors and maxError logic
        if (neighborCount < static_cast<unsigned>(minNeighbors))
            keep[ptIdx] = false;

        // TODO: add maxError check, isolated selection, etc., as in original filter

        pDlg.update(static_cast<int>(idx));
    }

    // Optionally, select isolated points if requested

    // Remove points that do not meet the criteria (keep excluded label points unfiltered)
    std::vector<unsigned> outputIndices;
    for (unsigned i = 0; i < m_cloud->size(); ++i)
    {
        if (!keep[i]) continue;
        outputIndices.push_back(i);
    }

    ccPointCloud* filteredCloud = m_cloud->partialClone(&outputIndices);
    if (!filteredCloud)
    {
        ccLog::Warning("[qNoiseF] Filtering failed!");
        return;
    }

    filteredCloud->setName(m_cloud->getName() + QString("_noiseFiltered"));
    m_cloud->getParent()->addChild(filteredCloud);
    filteredCloud->setDisplay(m_cloud->getDisplay());
    filteredCloud->prepareDisplayForRefresh();

    ccLog::Print(QString("[qNoiseF] Filtered cloud: %1 points").arg(filteredCloud->size()));
}

void qNoiseF::doAction()
{
	// Find selected cloud
	const ccHObject::Container& selected = getSelectedEntities();
	ccPointCloud* cloud = nullptr;
	for (ccHObject* obj : selected)
	{
		cloud = ccHObjectCaster::ToPointCloud(obj);
		if (cloud)
			break;
	}
	if (!cloud)
	{
		ccLog::Warning("[qNoiseF] Please select a point cloud.");
		return;
	}

	NoiseFDialog dlg;
	if (!dlg.exec())
		return;

	// --- Put your noise filter logic here ---
	// Use dialog getters: dlg.isRadiusMode(), dlg.getRadius(), dlg.getKNN(), dlg.getMinNeighbors(), dlg.getMaxError(), dlg.selectIsolated(), dlg.getExcludedLabel()
	// The dialog also allows you to add more options as needed.
	ccLog::Print("[qNoiseF] Ready to run filter with user settings.");
	// TODO: Implement the actual filtering, copying CloudCompare's core noise filter logic, 
	// and exclude points with 'excluded label' from neighbor calculations.
}