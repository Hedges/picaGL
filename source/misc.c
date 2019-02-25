#include "internal.h"

void glClear(GLbitfield mask)
{
	shaderProgramUse(&pglState->clearShader);

	uint32_t write_mask = 0;

	if(mask & GL_COLOR_BUFFER_BIT)
		write_mask |= GPU_WRITE_COLOR;
	if(mask & GL_DEPTH_BUFFER_BIT)
		write_mask |= GPU_WRITE_DEPTH;
	
	_picaAlphaTest(false, GPU_ALWAYS, 0);
	_picaDepthTestWriteMask(true, GPU_ALWAYS, write_mask);
	_picaCullMode(GPU_CULL_NONE);

	_picaTexUnitEnable(0x00);
	_picaTextureEnvSet(0, &pglState->texenv[PGL_TEXENV_UNTEXTURED]);
	_picaTextureEnvSet(1, &pglState->texenv[PGL_TEXENV_DUMMY]);

	_picaUniformFloat(GPU_VERTEX_SHADER, 0, (float*)&pglState->clearColor, 1);

	_picaAttribBuffersFormat(0, 0XFF, 0x0, 1);
	_picaImmediateBegin(GPU_TRIANGLE_STRIP);

	_picaFixedAttribute(-1.0,  1.0, pglState->clearDepth, 1.0f);
	_picaFixedAttribute(-1.0, -1.0, pglState->clearDepth, 1.0f);
	_picaFixedAttribute( 1.0,  1.0, pglState->clearDepth, 1.0f);
	_picaFixedAttribute( 1.0, -1.0, pglState->clearDepth, 1.0f);

	_picaImmediateEnd();

	shaderProgramUse(&pglState->basicShader);

	pglState->changes |= STATE_ALPHATEST_CHANGE;
	pglState->changes |= STATE_DEPTHTEST_CHANGE;
	pglState->changes |= STATE_CULL_CHANGE;
	pglState->changes |= STATE_TEXTURE_CHANGE;
}

void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	//We're passing this as a uniform...
	pglState->clearColor.a = red;
	pglState->clearColor.g = green;
	pglState->clearColor.b = blue;
	pglState->clearColor.r = alpha;
}

void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	uint8_t mask = (red & 0x1) | ((green & 0x1) << 1) | ((blue & 0x1) << 2) | ((alpha & 0x1) << 3);
	pglState->writeMask = (pglState->writeMask & 0x10) | mask;

	pglState->changes |= STATE_DEPTHTEST_CHANGE;
}

void glCullFace(GLenum mode)
{
	switch(mode)
	{
		case GL_FRONT:
			pglState->cullMode = GPU_CULL_FRONT_CCW;
			break;
		default:
			pglState->cullMode = GPU_CULL_BACK_CCW;
			break;
	}

	pglState->changes |= STATE_CULL_CHANGE;
}

void glFlush(void)
{
	u32* commandBuffer;
	u32  commandBuffer_size;

	_picaFinalize(true);

	GPUCMD_Split(&commandBuffer, &commandBuffer_size);

	GX_FlushCacheRegions (commandBuffer, commandBuffer_size * 4, (u32 *) __ctru_linear_heap, __ctru_linear_heap_size, NULL, 0);
	GX_ProcessCommandList(commandBuffer, commandBuffer_size * 4, 0x00);

	gspWaitForP3D();

	GPUCMD_SetBuffer(pglState->commandBuffer, COMMAND_BUFFER_LENGTH, 0);
}

void glFinish(void)
{
	glFlush();
	
}

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	pglState->viewportX = y;
	pglState->viewportY = x;
	pglState->viewportWidth = height;
	pglState->viewportHeight = width;

	pglState->changes |= STATE_VIEWPORT_CHANGE;
}

void glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
	pglState->scissorX = x;
	pglState->scissorY = y;
	pglState->scissorWidth = width;
	pglState->scissorHeight = height;
}