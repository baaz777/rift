#pragma once

/**
 * @struct BrushTransform
 * @brief Brush orientation for placement and preview.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Destination dimensions swap at 90 and 270 degrees. Inverse mapping undoes destination
 * flips before rotation.
 */
struct BrushTransform
{
    int width;     ///< Brush source width in tiles (pre-rotation).
    int height;    ///< Brush source height in tiles (pre-rotation).
    int rotation;  ///< 0, 90, 180, or 270 degrees (CCW, matching the editor R-key).
    bool flipX;
    bool flipY;
};

/**
 * @struct BrushSourceCoord
 * @brief Offset from the unrotated brush top-left, in tiles.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 */
struct BrushSourceCoord
{
    int sourceDx;  ///< Source column index (0..width-1).
    int sourceDy;  ///< Source row index (0..height-1).
};

/**
 * @fn BrushSourceCoord CalculateBrushSourceTile(int dx, int dy, const BrushTransform& bt)
 * @brief Maps a destination cell back through flips and counter-clockwise rotation.
 * @author Alex (<https://github.com/lextpf>)
 *
 * dx and dy must be inside the rotated footprint. At 90/270 degrees, dy ranges over
 * source width. At rotation 90 without flips, destination (0, 2) maps to source (0, 0).
 *
 * @verbatim
 *   source (rot 0, no flip)      rot 90 CCW           rot 180          rot 270 CCW
 *   dest is 3 wide, 2 tall       dest 2 x 3           dest 3 x 2       dest 2 x 3
 *
 *        A B C                     C F                 F E D              D A
 *        D E F                     B E                 C B A              E B
 *                                  A D                                    F C
 *
 *   flipX mirrors the DESTINATION columns, whatever the rotation:
 *
 *   rot 0 + flipX                rot 90 + flipX
 *        C B A                     F C
 *        F E D                     E B
 *                                  D A
 * @endverbatim
 */
inline BrushSourceCoord CalculateBrushSourceTile(int dx, int dy, const BrushTransform& bt)
{
    const int destW = (bt.rotation == 90 || bt.rotation == 270) ? bt.height : bt.width;
    const int destH = (bt.rotation == 90 || bt.rotation == 270) ? bt.width : bt.height;

    const int dxe = bt.flipX ? (destW - 1 - dx) : dx;
    const int dye = bt.flipY ? (destH - 1 - dy) : dy;

    BrushSourceCoord r{};
    if (bt.rotation == 90)
    {
        r.sourceDx = bt.width - 1 - dye;
        r.sourceDy = dxe;
    }
    else if (bt.rotation == 180)
    {
        r.sourceDx = bt.width - 1 - dxe;
        r.sourceDy = bt.height - 1 - dye;
    }
    else if (bt.rotation == 270)
    {
        r.sourceDx = dye;
        r.sourceDy = bt.height - 1 - dxe;
    }
    else
    {
        r.sourceDx = dxe;
        r.sourceDy = dye;
    }
    return r;
}
