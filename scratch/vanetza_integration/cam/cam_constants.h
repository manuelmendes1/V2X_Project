#pragma once

// CAM referencePosition uses WGS84 geographic coordinates.
// CamInput latitude and longitude are expressed in decimal degrees.
// The ETSI ITS CDD integer representation uses 1e-7 degree units.
//
// Keep the same scale in the ASN.1 builder and converter.
constexpr double kCamPositionScale = 1e7;
