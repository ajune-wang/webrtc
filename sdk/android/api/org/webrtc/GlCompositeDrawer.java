package org.webrtc;

import android.graphics.Matrix;

public class GlCompositeDrawer implements RendererCommon.GlDrawer {
  private static final String TAG = "GlCompositeDrawer";

  private static final RendererCommon.GlDrawer drawers[] = new RendererCommon.GlDrawer[2];

  /**
   *  Takes ownership of the drawers passed in.
   */
  public GlCompositeDrawer(
      RendererCommon.GlDrawer firstDrawer, RendererCommon.GlDrawer secondDrawer) {
    drawers[0] = firstDrawer;
    drawers[1] = secondDrawer;
  }

  private static float[] adjustedTexMatrix(
      float[] texMatrix, float frameAspectRatio, float layoutAspectRatio) {
    final float scaleX;
    final float scaleY;
    if (frameAspectRatio > layoutAspectRatio) {
      scaleX = layoutAspectRatio / frameAspectRatio;
      scaleY = 1f;
    } else {
      scaleX = 1f;
      scaleY = frameAspectRatio / layoutAspectRatio;
    }

    final Matrix drawMatrix = new Matrix();
    drawMatrix.preTranslate(0.5f, 0.5f);
    drawMatrix.preScale(scaleX, scaleY);
    drawMatrix.preTranslate(-0.5f, -0.5f);
    Matrix finalMatrix = RendererCommon.convertMatrixToAndroidGraphicsMatrix(texMatrix);
    finalMatrix.preConcat(drawMatrix);
    return RendererCommon.convertMatrixFromAndroidGraphicsMatrix(finalMatrix);
  }

  @Override
  public void drawOes(int oesTextureId, float[] texMatrix, int frameWidth, int frameHeight,
      int viewportX, int viewportY, int viewportWidth, int viewportHeight) {
    final float frameAspectRatio = frameWidth / (float) frameHeight;
    final float layoutAspectRatio = viewportWidth / (float) viewportHeight;
    final float[] texMatrix2 = adjustedTexMatrix(texMatrix, frameAspectRatio, layoutAspectRatio);

    drawers[0].drawOes(oesTextureId, texMatrix2, frameWidth, frameHeight, viewportX, viewportY,
        viewportWidth, viewportHeight);

    final int croppedViewportWidth =
        Math.min(viewportWidth, (viewportHeight * frameWidth) / frameHeight);
    final int croppedViewportHeight =
        Math.min(viewportHeight, (viewportWidth * frameHeight) / frameWidth);
    viewportX += (viewportWidth - croppedViewportWidth) / 2;
    viewportY += (viewportHeight - croppedViewportHeight) / 2;

    Logging.e("###",
        "OES viewport: " + viewportWidth + "x" + viewportHeight + ", frame: " + frameWidth + "x"
            + frameHeight + ", cropped viewport: " + croppedViewportWidth + "x"
            + croppedViewportHeight);

    drawers[1].drawOes(oesTextureId, texMatrix, frameWidth, frameHeight, viewportX, viewportY,
        croppedViewportWidth, croppedViewportHeight);
  }

  @Override
  public void drawRgb(int textureId, float[] texMatrix, int frameWidth, int frameHeight,
      int viewportX, int viewportY, int viewportWidth, int viewportHeight) {
    final float frameAspectRatio = frameWidth / (float) frameHeight;
    final float layoutAspectRatio = viewportWidth / (float) viewportHeight;
    final float[] texMatrix2 = adjustedTexMatrix(texMatrix, frameAspectRatio, layoutAspectRatio);

    drawers[0].drawRgb(textureId, texMatrix2, frameWidth, frameHeight, viewportX, viewportY,
        viewportWidth, viewportHeight);

    final int croppedViewportWidth =
        Math.min(viewportWidth, (viewportHeight * frameWidth) / frameHeight);
    final int croppedViewportHeight =
        Math.min(viewportHeight, (viewportWidth * frameHeight) / frameWidth);
    viewportX += (viewportWidth - croppedViewportWidth) / 2;
    viewportY += (viewportHeight - croppedViewportHeight) / 2;

    Logging.e("###",
        "RGB viewport: " + viewportWidth + "x" + viewportHeight + ", frame: " + frameWidth + "x"
            + frameHeight + ", cropped viewport: " + croppedViewportWidth + "x"
            + croppedViewportHeight);

    drawers[1].drawRgb(textureId, texMatrix, frameWidth, frameHeight, viewportX, viewportY,
        croppedViewportWidth, croppedViewportHeight);

    // }
  }

  @Override
  public void drawYuv(int[] yuvTextures, float[] texMatrix, int frameWidth, int frameHeight,
      int viewportX, int viewportY, int viewportWidth, int viewportHeight) {
    final float frameAspectRatio = frameWidth / (float) frameHeight;
    final float layoutAspectRatio = viewportWidth / (float) viewportHeight;
    final float[] texMatrix2 = adjustedTexMatrix(texMatrix, frameAspectRatio, layoutAspectRatio);

    drawers[0].drawYuv(yuvTextures, texMatrix2, frameWidth, frameHeight, viewportX, viewportY,
        viewportWidth, viewportHeight);

    final int croppedViewportWidth =
        Math.min(viewportWidth, (viewportHeight * frameWidth) / frameHeight);
    final int croppedViewportHeight =
        Math.min(viewportHeight, (viewportWidth * frameHeight) / frameWidth);
    viewportX += (viewportWidth - croppedViewportWidth) / 2;
    viewportY += (viewportHeight - croppedViewportHeight) / 2;

    Logging.e("###",
        "YUV viewport: " + viewportWidth + "x" + viewportHeight + ", frame: " + frameWidth + "x"
            + frameHeight + ", cropped viewport: " + croppedViewportWidth + "x"
            + croppedViewportHeight);

    drawers[1].drawYuv(yuvTextures, texMatrix, frameWidth, frameHeight, viewportX, viewportY,
        croppedViewportWidth, croppedViewportHeight);

    // }
  }

  @Override
  public void release() {
    for (RendererCommon.GlDrawer drawer : drawers) {
      drawer.release();
    }
  }
}
