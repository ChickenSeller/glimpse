# kImageAnnotator in Glimpse

This is [kImageAnnotator](https://github.com/ksnip/kImageAnnotator) by Damir
Porobic and contributors, licensed under the GNU Lesser General Public
License 3.0 (see `LICENSE`). Glimpse's changes to it are under the same
license.

## Base

The release archive of v0.7.2, unchanged, is the commit "Add kImageAnnotator
0.7.2 as published" in Glimpse's history
(<https://github.com/ksnip/kImageAnnotator/archive/refs/tags/v0.7.2.tar.gz>,
SHA-256 `7eb593d975b1590a184354ef68dbc3c26479d58eaea00de461d73695176f623c`).
Glimpse no longer follows upstream; later upstream changes are taken over
only when needed, and listed below.

## Glimpse's changes

- **Exports on Windows** (`include/kImageAnnotator/KImageAnnotatorExport.h`):
  classes are declared dllexport/dllimport on Windows too. Without it MinGW
  exports everything on its own, users take member functions from import
  stubs, and connecting to a signal by pointer fails.
- **Settings file** (`src/backend/SettingsAdapter.*`): the tools' settings go
  to the file named by the application's `glimpseSettingsFile` property
  (Glimpse's own settings file) when it names one.
- **Canvas painting** (`src/annotations/misc/CanvasPainter.cpp`): the view
  gets Glimpse's checkerboard (8 x 8 px cells, fixed to the screen at any
  zoom) and, while the view's `glimpseFramed` property is set, a thin frame
  just outside the area that is saved.
- **Crop and cut scale** (`src/common/provider/DevicePixelRatioScaler.cpp`):
  1, since Glimpse loads images pixel for pixel; with the screen's device
  pixel ratio, selections were cropped too far right and down on scaled
  screens.

## Glimpse's tools

- **Triangle** (`src/annotations/items/AnnotationTriangle.*`, shortcut G, with
  Rectangle and Ellipse): an isosceles triangle filling the dragged
  rectangle, its apex on the edge where the drag started, so dragging
  upwards draws it pointing down. Same settings as the rectangle. Added last
  to the `Tools` enum, since tool settings are stored by the tool's number.
- **Curve and Curved Arrow** (`src/annotations/items/AbstractAnnotationCurve.*`,
  `AnnotationCurve.*`, `AnnotationCurveArrow.*`,
  `src/annotations/modifiers/resizeHandles/CurveResizeHandles.*`,
  `src/annotations/undo/CurveBendsCommand.*`; shortcuts V and Y, with Arrow
  and Line): drawn straight like a line, with one bend point halfway. The
  curve passes smoothly through its bend points in order (a Catmull-Rom
  spline, drawn as cubic Bezier curves); a double-click on a selected curve
  adds a bend point there, or removes the one it is on, as an undoable step.
  Bend points are kept relative to the two ends, so they follow them. The
  curved arrow's head points along the curve at its end; its shaft is
  stroked only, its head stroked and filled, since kImageAnnotator's own
  painting would fill the area under the curve. Added last to the `Tools`
  enum.
- **Adjusting a curve right after drawing it**
  (`src/annotations/core/AnnotationArea.*`,
  `src/annotations/modifiers/AnnotationItemModifier.*`): a new curve is
  selected (`requiresSelectionAfterCreation`), and under a drawing tool a
  press on the selection or its handles adjusts it instead of drawing a new
  item.
