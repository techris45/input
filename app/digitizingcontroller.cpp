#include "digitizingcontroller.h"

#include "qgslinestring.h"
#include "qgsvectorlayer.h"
#include "qgswkbtypes.h"

#include "qgsquickutils.h"

DigitizingController::DigitizingController(QObject *parent)
  : QObject(parent)
  , mMapSettings(nullptr)
{
  mRecordingModel = new QgsQuickAttributeModel( this );
}

void DigitizingController::setPositionKit(QgsQuickPositionKit *kit)
{
  if (mPositionKit)
    disconnect(mPositionKit, &QgsQuickPositionKit::positionChanged, this, &DigitizingController::onPositionChanged);

  mPositionKit = kit;

  if (mPositionKit)
    connect(mPositionKit, &QgsQuickPositionKit::positionChanged, this, &DigitizingController::onPositionChanged);

  emit positionKitChanged();
}

QgsQuickFeatureLayerPair DigitizingController::featureLayerPair() const
{
    return mRecordingModel->featureLayerPair();
}

void DigitizingController::setFeatureLayerPair(QgsQuickFeatureLayerPair pair)
{
    if ( pair == mRecordingModel->featureLayerPair() )
        return;

      mRecordingModel->setFeatureLayerPair( pair );
      emit layerChanged();
}

QgsVectorLayer *DigitizingController::layer() const
{
  return mRecordingModel->featureLayerPair().layer();
}

void DigitizingController::setLayer(QgsVectorLayer *layer)
{
  if ( layer == mRecordingModel->featureLayerPair().layer() )
    return;

  QgsQuickFeatureLayerPair pair(mRecordingModel->featureLayerPair().featureRef(), layer);
  mRecordingModel->setFeatureLayerPair( pair );
  emit layerChanged();
}

bool DigitizingController::hasLineGeometry(QgsVectorLayer *layer) const
{
  return layer && layer->geometryType() == QgsWkbTypes::LineGeometry;
}

bool DigitizingController::hasPolygonGeometry(QgsVectorLayer *layer) const
{
  return layer && layer->geometryType() == QgsWkbTypes::PolygonGeometry;
}

bool DigitizingController::hasPointGeometry(QgsVectorLayer *layer) const
{
  return layer && layer->geometryType() == QgsWkbTypes::PointGeometry;
}

void DigitizingController::fixZ(QgsPoint* point) const {
    Q_ASSERT(point);

    if ( !featureLayerPair().layer() )
        return;

    bool layerIs3D = QgsWkbTypes::hasZ(featureLayerPair().layer()->wkbType());
    bool pointIs3D = QgsWkbTypes::hasZ(point->wkbType());

    if (layerIs3D) {
        if (!pointIs3D) {
            point->addZValue();
        }
    } else /* !layerIs3D */ {
        if (pointIs3D) {
            point->dropZValue();
        }
    }
}

QgsCoordinateTransform DigitizingController::tranformer() const
{
  QgsCoordinateTransformContext context;
  if ( mMapSettings )
     context = mMapSettings->transformContext();

  QgsCoordinateTransform transform( QgsCoordinateReferenceSystem( "EPSG:4326" ),
                                    featureLayerPair().layer()->crs(),
                                    context);
  return transform;
}

bool DigitizingController::manualRecording() const
{
    return mManualRecording;
}

void DigitizingController::setManualRecording(bool manualRecording)
{
    mManualRecording = manualRecording;
    emit manualRecordingChanged();
}

int DigitizingController::lineRecordingInterval() const
{
    return mLineRecordingInterval;
}

void DigitizingController::setLineRecordingInterval(int lineRecordingInterval)
{
    mLineRecordingInterval = lineRecordingInterval;
    emit lineRecordingIntervalChanged();
}

QgsQuickFeatureLayerPair DigitizingController::pointFeatureFromPoint(const QgsPoint &point)
{
    if ( !featureLayerPair().layer() )
        return QgsQuickFeatureLayerPair();

    if (!mMapSettings) {
        return QgsQuickFeatureLayerPair();
    }

    QgsPointXY layerPoint = mMapSettings->mapSettings().mapToLayerCoordinates(featureLayerPair().layer(), QgsPointXY(point.x(), point.y()));
    QgsPoint* mapPoint = new QgsPoint( layerPoint );
    fixZ(mapPoint);
    QgsGeometry geom( mapPoint );

    QgsFeature f;
    f.setGeometry( geom );
    f.setFields( featureLayerPair().layer()->fields() );
    QgsAttributes attrs( f.fields().count() );
    f.setAttributes( attrs );
    return QgsQuickFeatureLayerPair(f, featureLayerPair().layer());
}

void DigitizingController::startRecording()
{
  mRecordedPoints.clear();

  mRecording = true;
  emit recordingChanged();
}

void DigitizingController::stopRecording()
{
  mRecording = false;
  emit recordingChanged();
}

void DigitizingController::onPositionChanged()
{
  if (mManualRecording)
    return;

  if ( !mRecording )
    return;

  if ( !mPositionKit->hasPosition() )
    return;


  QgsPoint point = mPositionKit->position();
  fixZ(&point);

  if (mLastTimeRecorded.addSecs(mLineRecordingInterval) <= QDateTime::currentDateTime()) {
      mLastTimeRecorded = QDateTime::currentDateTime();
      mRecordedPoints.append( point );
  } else {
      if (!mRecordedPoints.isEmpty()) {
          mRecordedPoints.last().setX(point.x());
          mRecordedPoints.last().setY(point.y());
      }
  }
  // update geometry so we can use the model for highlight in map
  mRecordingModel->setFeatureLayerPair(lineFeature());
}

QgsQuickFeatureLayerPair DigitizingController::lineFeature()
{
  if ( !featureLayerPair().layer() )
    return QgsQuickFeatureLayerPair();

  if ( mRecordedPoints.isEmpty() )
    return QgsQuickFeatureLayerPair();

  QgsLineString *linestring = new QgsLineString;
  Q_FOREACH ( const QgsPoint &pt, mRecordedPoints )
    linestring->addVertex( pt );

  QgsGeometry geom( linestring );

  geom.transform(tranformer());

  QgsFeature f;
  f.setGeometry( geom );
  f.setFields( featureLayerPair().layer()->fields() );
  QgsAttributes attrs( f.fields().count() );
  f.setAttributes( attrs );

  return QgsQuickFeatureLayerPair(f, featureLayerPair().layer());
}

QgsPoint DigitizingController::pointFeatureMapCoordinates(QgsQuickFeatureLayerPair pair)
{
    if ( !pair.layer() )
      return QgsPoint();

    QgsPointXY res = mMapSettings->mapSettings().layerToMapCoordinates(pair.layer(), QgsPoint(pair.feature().geometry().asPoint()));
    return QgsPoint(res);
}

QgsQuickFeatureLayerPair DigitizingController::changePointGeometry(QgsQuickFeatureLayerPair pair, QgsPoint point)
{
    QgsPointXY layerPointXY = mMapSettings->mapSettings().mapToLayerCoordinates(pair.layer(), QgsPointXY(point.x(), point.y()));
    QgsPoint* layerPoint = new QgsPoint( layerPointXY );
    fixZ(layerPoint);
    QgsGeometry geom( layerPoint );

    pair.featureRef().setGeometry(geom);
    return pair;
}

void DigitizingController::addPoint(const QgsPoint &point)
{
    if ( !mRecording )
      return;

    QgsPointXY layerPoint = mMapSettings->mapSettings().mapToLayerCoordinates(featureLayerPair().layer(), QgsPointXY(point.x(), point.y()));
    QgsPoint* mapPoint = new QgsPoint( layerPoint );
    fixZ(mapPoint);
    mRecordedPoints.append( *mapPoint );

    // update geometry so we can use the model for highlight in map
    mRecordingModel->setFeatureLayerPair(lineFeature());
}

void DigitizingController::removeLastPoint()
{
    if (mRecordedPoints.isEmpty())
        return;

    if (mRecordedPoints.size() == 1) {
        // cancel recording
        mRecording = false;
        emit recordingChanged();

        return;
    }

    mRecordedPoints.removeLast();
    mRecordingModel->setFeatureLayerPair(lineFeature());
}
