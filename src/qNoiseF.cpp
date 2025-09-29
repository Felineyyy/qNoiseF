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
#include <vector>
#include <algorithm>

qNoiseF::qNoiseF(QObject* parent)
    : QObject(parent)
    , ccStdPluginInterface(":/CC/plugin/qNoiseF/info.json")
    , m_action(nullptr)
{
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

void qNoiseF::doAction()
{
    // Get selected entities from the app interface
    ccHObject::Container selectedEntities = m_app ? m_app->getSelectedEntities() : ccHObject::Container();
    
    ccPointCloud* cloud = nullptr;
    for (ccHObject* obj : selectedEntities)
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

    // Show dialog to get parameters
    NoiseFDialog dlg(m_app ? m_app->getMainWindow() : nullptr);
    if (dlg.exec() != QDialog::Accepted)
        return;

    // Run the noise filter
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

    // Get label scalar field if it exists
    int labelSfIdx = cloud->getScalarFieldIndexByName("Label");
    ccScalarField* labelSf = nullptr;
    if (labelSfIdx >= 0)
    {
        labelSf = static_cast<ccScalarField*>(cloud->getScalarField(labelSfIdx));
        ccLog::Print(QString("[qNoiseF] Found Label scalar field, excluding label: %1 from neighbor calculations").arg(excludedLabel));
    }
    else
    {
        ccLog::Warning("[qNoiseF] No 'Label' scalar field found. Processing all points.");
    }

    // Build octree if needed
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

    // Progress dialog
    QWidget* parentWidget = m_app ? m_app->getMainWindow() : nullptr;
    ccProgressDialog pDlg(true, parentWidget);
    pDlg.setMethodTitle("Noise Filter with Label Exclusion");
    pDlg.setInfo("Filtering points...");
    pDlg.start();

    ccLog::Print(QString("[qNoiseF] Processing %1 points (radius mode: %2, max error: %3, min neighbors: %4)")
                 .arg(pointCount).arg(useRadius).arg(maxError).arg(minNeighbors));

    // Mark points to keep
    std::vector<bool> keepPoint(pointCount, true);
    unsigned filteredCount = 0;
    
    // Process each point
    for (unsigned i = 0; i < pointCount; ++i)
    {
        if (pDlg.wasCanceled())
        {
            ccLog::Warning("[qNoiseF] Process canceled by user");
            pDlg.stop();
            return;
        }

        const CCVector3* P = cloud->getPoint(i);
        
        // Build neighbor cloud (excluding points with excluded label)
        CCCoreLib::ReferenceCloud neighbors(cloud);
        
        if (useRadius)
        {
            // Radius-based search
            CCCoreLib::DgmOctree::NeighboursSet neighborsSet;
            octree->getPointsInSphericalNeighbourhood(*P, radius, neighborsSet, 1);
            
            for (const auto& n : neighborsSet)
            {
                unsigned idx = n.pointIndex;
                if (idx == i) continue;
                if (labelSf && static_cast<int>(labelSf->getValue(idx)) == excludedLabel)
                    continue;
                
                neighbors.addPointIndex(idx);
            }
        }
        else
        {
            // KNN-based search
            CCCoreLib::DgmOctree::NeighboursSet neighborsSet;
            int searchK = knn + 100; // Search extra to account for excluded labels
            double maxSquareDist = 0;
            
            octree->findPointNeighbourhood(P, &neighborsSet, static_cast<unsigned>(searchK), 0, maxSquareDist);
            
            for (const auto& n : neighborsSet)
            {
                unsigned idx = n.pointIndex;
                if (idx == i) continue;
                if (labelSf && static_cast<int>(labelSf->getValue(idx)) == excludedLabel)
                    continue;
                
                neighbors.addPointIndex(idx);
                
                if (static_cast<int>(neighbors.size()) >= knn)
                    break;
            }
        }

        unsigned neighborCount = neighbors.size();

        // Check if point is isolated (optional removal)
        if (removeIsolated && static_cast<int>(neighborCount) < minNeighbors)
        {
            keepPoint[i] = false;
            filteredCount++;
            continue;
        }

        // Need at least 3 neighbors to fit a plane
        if (neighborCount < 3)
        {
            // Can't fit plane, keep the point by default
            continue;
        }

        // Fit a plane to the neighbors
        CCCoreLib::Neighbourhood neighbourhood(&neighbors);
        
        const CCVector3* planeEquation = neighbourhood.getLSPlane();
        if (!planeEquation)
        {
            // Plane fitting failed, keep the point
            continue;
        }

        // Calculate distance from point to fitted plane
        // Plane equation: N.x + d = 0, where planeEquation[0..2] is normal N and planeEquation[3] is d
        const PointCoordinateType* N = planeEquation;
        PointCoordinateType d = N[3];
        
        // Distance = |N.P + d| / |N|
        PointCoordinateType distance = std::abs(N[0] * P->x + N[1] * P->y + N[2] * P->z + d);
        
        // Check if distance exceeds max error threshold
        if (maxError > 0.0)
        {
            if (distance > maxError)
            {
                keepPoint[i] = false;
                filteredCount++;
            }
        }

        if (i % 1000 == 0)
        {
            pDlg.update(static_cast<float>(i) / pointCount * 100.0f);
        }
    }

    pDlg.stop();

    ccLog::Print(QString("[qNoiseF] Filtered %1 points out of %2 total points")
                 .arg(filteredCount).arg(pointCount));

    // Create filtered cloud (points that passed the filter)
    CCCoreLib::ReferenceCloud refCloud(cloud);
    
    for (unsigned i = 0; i < pointCount; ++i)
    {
        if (keepPoint[i])
            refCloud.addPointIndex(i);
    }

    if (refCloud.size() == 0)
    {
        ccLog::Warning("[qNoiseF] All points were filtered!");
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
