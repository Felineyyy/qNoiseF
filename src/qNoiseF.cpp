// qNoiseF.cpp
#include "qNoiseF.h"
#include "NoiseFDialog.h"

#include <ccPointCloud.h>
#include <ccScalarField.h>
#include <ccProgressDialog.h>
#include <ccLog.h>
#include <ccOctree.h>
#include <ReferenceCloud.h>
#include <GenericProgressCallback.h>
#include <DgmOctree.h>
#include <Neighbourhood.h>

#include <QIcon>
#include <QWidget>
#include <cmath>
#include <vector>

qNoiseF::qNoiseF(QObject* parent)
    : QObject(parent)
    , ccStdPluginInterface(":/CC/plugin/qNoiseF/info.json")
    , m_action(nullptr)
{
}

QIcon qNoiseF::getIcon() const
{
    return QIcon(":/CC/plugin/qNoiseF/icon.png");
}

void qNoiseF::onNewSelection(const ccHObject::Container& selectedEntities)
{
    if (m_action)
    {
        bool hasCloud = false;
        for (ccHObject* entity : selectedEntities)
        {
            if (ccHObjectCaster::ToPointCloud(entity))
            {
                hasCloud = true;
                break;
            }
        }
        m_action->setEnabled(hasCloud);
    }
}

QList<QAction*> qNoiseF::getActions()
{
    if (!m_action)
    {
        m_action = new QAction(getName(), this);
        m_action->setToolTip(getDescription());
        m_action->setIcon(getIcon());
        connect(m_action, &QAction::triggered, this, &qNoiseF::doAction);
    }
    return { m_action };
}

void qNoiseF::doAction()
{
    // Retrieve the first selected point cloud
    ccPointCloud* cloud = nullptr;
    ccHObject::Container selectedEntities = m_app ? m_app->getSelectedEntities() : ccHObject::Container();
    for (ccHObject* obj : selectedEntities)
    {
        if (obj && obj->isKindOf(CC_TYPES::POINT_CLOUD))
        {
            cloud = static_cast<ccPointCloud*>(obj);
            break;
        }
    }

    if (!cloud)
    {
        ccLog::Warning("[qNoiseF] No valid point cloud selected!");
        return;
    }

    // Parent is set to nullptr to avoid forward-declaration/cast issues
    NoiseFDialog dlg(nullptr);
    if (dlg.exec() != QDialog::Accepted)
        return;

    // Run filter: use dialog params (excludedLabel provided by dialog)
    doNoiseFilter(
        cloud,
        dlg.isRadiusMode(),
        dlg.getRadius(),
        dlg.getKNN(),
        dlg.getMinNeighbors(),
        dlg.getMaxError(),
        dlg.selectIsolated(),
        dlg.getExcludedLabel()
    );
}

void qNoiseF::doNoiseFilter(ccPointCloud* cloud, bool useRadius, double radius, int knn,
                            int minNeighbors, double maxError,
                            bool removeIsolated, int excludedLabel)
{
    if (!cloud)
        return;

    unsigned pointCount = cloud->size();
    if (pointCount == 0)
    {
        ccLog::Warning("[qNoiseF] Cloud is empty!");
        return;
    }

    // === IMPORTANT CHANGE ===
    // Use the currently displayed (selected) scalar field on the current cloud,
    // do NOT search for a scalar field named "Label".
    ccScalarField* labelSf = cloud->getCurrentDisplayedScalarField();
    if (labelSf)
    {
        ccLog::Print(QString("[qNoiseF] Using currently displayed scalar field as label SF (size=%1). Excluding label value: %2")
                     .arg(labelSf->currentSize()).arg(excludedLabel));
    }
    else
    {
        ccLog::Warning("[qNoiseF] No scalar field currently displayed on the selected cloud. All points will be processed (no label exclusion).");
    }

    // Build / get octree
    ccOctree::Shared octree = cloud->getOctree();
    if (!octree)
    {
        octree = cloud->computeOctree();
        if (!octree)
        {
            ccLog::Error("[qNoiseF] Failed to compute octree!");
            return;
        }
    }

    // Progress dialog (nullptr parent to avoid QMainWindow forward-declare issues)
    ccProgressDialog pDlg(true, nullptr);
    pDlg.setMethodTitle("Noise Filter with Label Exclusion");
    pDlg.setInfo("Filtering points...");
    pDlg.start();

    std::vector<bool> keepPoint(pointCount, true);
    unsigned filteredCount = 0;

    // Main loop
    for (unsigned i = 0; i < pointCount; ++i)
    {
        if ((i % 1000) == 0)
        {
            if (pDlg.wasCanceled())
            {
                ccLog::Warning("[qNoiseF] Process canceled by user");
                pDlg.stop();
                return;
            }
            pDlg.update(static_cast<float>(i) / pointCount * 100.0f);
        }

        const CCVector3* P = cloud->getPoint(i);

        // Protect points that themselves have the excluded label value:
        if (labelSf && excludedLabel >= 0 && static_cast<int>(labelSf->getValue(i)) == excludedLabel)
        {
            // keep point, skip filtering for this point
            continue;
        }

        // Build neighbor list excluding points whose SF value == excludedLabel
        CCCoreLib::ReferenceCloud neighbors(cloud);

        if (useRadius)
        {
            // Radius-based search
            CCCoreLib::DgmOctree::NeighboursSet neighboursSet;
            unsigned char lvl = octree->findBestLevelForAGivenNeighbourhoodSizeExtraction(static_cast<PointCoordinateType>(radius));
            // API: getPointsInSphericalNeighbourhood(point, radius, neighboursSet, level)
            int foundNeighbors = octree->getPointsInSphericalNeighbourhood(*P, static_cast<PointCoordinateType>(radius), neighboursSet, lvl);

            if (foundNeighbors > 0)
            {
                for (const auto& pd : neighboursSet)
                {
                    unsigned idx = pd.pointIndex;
                    if (idx == i) // skip self
                        continue;

                    if (labelSf && excludedLabel >= 0 && static_cast<int>(labelSf->getValue(idx)) == excludedLabel)
                        continue;

                    neighbors.addPointIndex(idx);
                    if (static_cast<int>(neighbors.size()) >= knn)
                        break;
                }
            }
        }
        else
        {
            // KNN search: request more candidate neighbors to allow for excluded-label skipping
            int searchK = std::max(3, knn * 3);
            unsigned char lvlK = octree->findBestLevelForAGivenPopulationPerCell(static_cast<unsigned>(searchK));

            // findPointNeighbourhood fills a ReferenceCloud provided by the caller
            CCCoreLib::ReferenceCloud candidateRef(cloud);
            double maxSquareDist = 0;
            unsigned found = octree->findPointNeighbourhood(P, &candidateRef, static_cast<unsigned>(searchK), lvlK, maxSquareDist);

            if (found > 0)
            {
                for (unsigned ci = 0; ci < candidateRef.size(); ++ci)
                {
                    unsigned idx = candidateRef.getPointGlobalIndex(ci);
                    if (idx == i)
                        continue;

                    if (labelSf && excludedLabel >= 0 && static_cast<int>(labelSf->getValue(idx)) == excludedLabel)
                        continue;

                    neighbors.addPointIndex(idx);
                    if (static_cast<int>(neighbors.size()) >= knn)
                        break;
                }
            }
        }

        unsigned neighborCount = neighbors.size();

        // Isolation removal
        if (removeIsolated && static_cast<int>(neighborCount) < minNeighbors)
        {
            keepPoint[i] = false;
            ++filteredCount;
            continue;
        }

        // Need at least 3 neighbors for plane fitting
        if (neighborCount < 3)
        {
            if (removeIsolated)
            {
                keepPoint[i] = false;
                ++filteredCount;
            }
            continue;
        }

        // Fit plane
        CCCoreLib::Neighbourhood neighbourhood(&neighbors);
        const PointCoordinateType* planeEquation = neighbourhood.getLSPlane();
        if (!planeEquation)
            continue;

        // planeEquation[0..2] = normal, [3] = d (N.x + d = 0)
        const PointCoordinateType* N = planeEquation;
        PointCoordinateType d = N[3];

        PointCoordinateType normalLength = static_cast<PointCoordinateType>(std::sqrt(N[0] * N[0] + N[1] * N[1] + N[2] * N[2]));
        PointCoordinateType distance = 0;
        if (normalLength > 0)
            distance = std::abs(N[0] * P->x + N[1] * P->y + N[2] * P->z + d) / normalLength;

        if (maxError > 0.0 && distance > maxError)
        {
            keepPoint[i] = false;
            ++filteredCount;
        }
    }

    pDlg.stop();

    ccLog::Print(QString("[qNoiseF] Filtered %1 points out of %2 total points").arg(filteredCount).arg(pointCount));

    if (filteredCount == pointCount)
    {
        ccLog::Warning("[qNoiseF] All points were filtered!");
        return;
    }

    // Build result ReferenceCloud from kept points
    CCCoreLib::ReferenceCloud refCloud(cloud);
    for (unsigned i = 0; i < pointCount; ++i)
    {
        if (keepPoint[i])
            refCloud.addPointIndex(i);
    }

    if (refCloud.size() == 0)
    {
        ccLog::Warning("[qNoiseF] No points remain after filtering!");
        return;
    }

    ccPointCloud* filteredCloud = cloud->partialClone(&refCloud);
    if (!filteredCloud)
    {
        ccLog::Error("[qNoiseF] Failed to create filtered cloud!");
        return;
    }

    filteredCloud->setName(cloud->getName() + QString(".filtered"));
    filteredCloud->setDisplay(cloud->getDisplay());

    if (cloud->getParent())
        cloud->getParent()->addChild(filteredCloud);

    if (m_app)
        m_app->addToDB(filteredCloud);

    filteredCloud->prepareDisplayForRefresh();

    ccLog::Print(QString("[qNoiseF] Created filtered cloud with %1 points").arg(filteredCloud->size()));
}
