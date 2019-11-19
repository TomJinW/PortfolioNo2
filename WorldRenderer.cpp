WorldRenderer::WorldRenderer(RenderingContext* rctx, DepthProvider* depthProvider, ImageProvider* imageProvider, HenshinDetector* henshinDetector)
: AbstractOpenGLRenderer(rctx)
{
	m_depthProvider = depthProvider;
	m_imageProvider = imageProvider;
	m_henshinDetector = henshinDetector;

	m_width = X_RES;
	m_height = Y_RES;

	// allocate working buffers
	int numPoints = getNumPoints();
	m_vertexBuf = new M3DVector3f[numPoints];
	m_colorBuf = new M3DVector4f[numPoints];

	// pre-set values on working buffers
	M3DVector3f* vp = m_vertexBuf;
	M3DVector4f* cp = m_colorBuf;
	for (int iy = 0; iy < m_height; iy++) {
		float y = 1.0f - iy * 2.0f / m_height;
		for (int ix = 0; ix < m_width; ix++) {
			float x = 1.0f - ix * 2.0f / m_width;
			(*vp)[0] = x;
			(*vp)[1] = (*vp)[2] = 0;
			vp++;
			(*cp)[0] = (*cp)[1] = (*cp)[2] = 0;
			(*cp)[3] = 1; // alpha is always 1.0
			cp++;
		}
	}

	m_batch.init(numPoints);

	// texture for henshin glow
	m_henshinGlowTextureID = readAlphaTexture("sparkAlpha.png");

	m_depthAdjustment = DEFAULT_DEPTH_ADJUSTMENT;
}
