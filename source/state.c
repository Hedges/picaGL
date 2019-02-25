#include "internal.h"
#include "vshader_shbin.h"
#include "clear_shbin.h"

picaGLState *pglState;

void pglState_init()
{
	//pglState->vertexCache = sequential_buffer_create(0x280000);
	pglState->geometryBuffer = linearAlloc(GEOMETRY_BUFFER_SIZE);

	pglState->colorBuffer = vramAlloc(400 * 240 * 4);
	pglState->depthBuffer = vramAlloc(400 * 240 * 4);

	pglState->commandBuffer = linearAlloc(COMMAND_BUFFER_SIZE);
	GPUCMD_SetBuffer(pglState->commandBuffer, COMMAND_BUFFER_LENGTH, 0);

	pglState->textures = malloc(sizeof(TextureObject) * MAX_TEXTURES);

	pglState->basicShader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);

	shaderProgramInit(&pglState->basicShader);
	shaderProgramSetVsh(&pglState->basicShader, &pglState->basicShader_dvlb->DVLE[0]);
	shaderProgramUse(&pglState->basicShader);

	pglState->clearShader_dvlb = DVLB_ParseFile((u32*)clear_shbin, clear_shbin_size);

	shaderProgramInit(&pglState->clearShader);
	shaderProgramSetVsh(&pglState->clearShader, &pglState->clearShader_dvlb->DVLE[0]);

	_picaRenderBuffer(pglState->colorBuffer, pglState->depthBuffer);
	
	_picaAttribBuffersLocation((void*)__ctru_linear_heap);
}

void pglState_default()
{
	for(int i = 0; i < 4; i++)
		TextureEnv_reset(&pglState->texenv[i]);

	pglState->texenv[PGL_TEXENV_UNTEXTURED].src_rgb	= GPU_TEVSOURCES(GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
	pglState->texenv[PGL_TEXENV_UNTEXTURED].op_rgb		= GPU_TEVOPERANDS(0,0,0);
	pglState->texenv[PGL_TEXENV_UNTEXTURED].func_rgb	= GPU_REPLACE;
	pglState->texenv[PGL_TEXENV_UNTEXTURED].src_alpha	= pglState->texenv[PGL_TEXENV_UNTEXTURED].src_rgb;
	pglState->texenv[PGL_TEXENV_UNTEXTURED].op_alpha	= pglState->texenv[PGL_TEXENV_UNTEXTURED].op_rgb;
	pglState->texenv[PGL_TEXENV_UNTEXTURED].func_alpha	= pglState->texenv[PGL_TEXENV_UNTEXTURED].func_rgb;

	pglState->depthmapNear 	= 0.0f;
	pglState->depthmapFar 	= 1.0f;
	pglState->polygonOffset = 0.0f;

	glDisable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glDisable(GL_ALPHA_TEST);
	glDisable(GL_STENCIL_TEST);

	glEnableClientState(GL_VERTEX_ARRAY);

	glClearDepth(1.0);
	glMatrixMode(GL_MODELVIEW);

	pglState->writeMask = GPU_WRITE_ALL;

	glActiveTexture(1);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glActiveTexture(0);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	_picaEarlyDepthTest(false);

	for(int i = 1; i < 6; i++)
		_picaTextureEnvSet(i, &pglState->texenv[PGL_TEXENV_DUMMY]);

	pglState->changes = 0xffffff;
}

void pglState_flush()
{
	static matrix4x4 matrix_mvp;

	if(pglState->changes & STATE_VIEWPORT_CHANGE)
	{
		_picaViewport(pglState->viewportX, pglState->viewportY, pglState->viewportWidth, pglState->viewportHeight);
	}

	if(pglState->changes & STATE_STENCIL_CHANGE)
	{
		_picaStencilTest(pglState->stencilTestState, pglState->stencilTestFunction, pglState->stencilTestReference, pglState->stencilBufferMask, pglState->stencilWriteMask);
		_picaStencilOp(pglState->stencilOpFail, pglState->stencilOpZFail, pglState->stencilOpZPass);
	}

	if(pglState->changes & STATE_ALPHATEST_CHANGE)
	{
		_picaAlphaTest(pglState->alphaTestState, pglState->alphaTestFunction, pglState->alphaTestReference);
	}

	if(pglState->changes & STATE_BLEND_CHANGE)
	{
		if(pglState->blendState)
		{
			_picaBlendFunction(pglState->blendEquation, pglState->blendEquation, pglState->blendSrcFunction, pglState->blendDstFunction, pglState->blendSrcFunction, pglState->blendDstFunction);
			_picaBlendColor(pglState->blendColor);
		}
		else
		{
			_picaLogicOp(GPU_LOGICOP_COPY);
		}
	}

	if(pglState->changes & STATE_DEPTHMAP_CHANGE)
	{
		_picaDepthMap(pglState->depthmapNear, pglState->depthmapFar, pglState->polygonOffset);
	}

	if(pglState->changes & STATE_DEPTHTEST_CHANGE)
	{
		_picaDepthTestWriteMask(pglState->depthTestState, pglState->depthTestFunction, pglState->writeMask);
	}

	if(pglState->changes & STATE_CULL_CHANGE)
	{
		_picaCullMode(pglState->cullState ? pglState->cullMode : GPU_CULL_NONE);
	}

	if(pglState->changes & STATE_TEXTURE_CHANGE)
	{
		uint16_t texunit_enable_mask = 0;

		if(pglState->texUnitState[0] && pglState->textures[pglState->textureBound[0]].data)
		{
			texunit_enable_mask |= 0x01;
			_picaTextureEnvSet(0, &pglState->texenv[0]);
			_picaTextureObjectSet(GPU_TEXUNIT0, &pglState->textures[pglState->textureBound[0]]);
		}
		else
			_picaTextureEnvSet(0, &pglState->texenv[PGL_TEXENV_UNTEXTURED]);

		if(pglState->texUnitState[1] && pglState->textures[pglState->textureBound[1]].data)
		{
			texunit_enable_mask |= 0x02;
			_picaTextureEnvSet(1, &pglState->texenv[1]);
			_picaTextureObjectSet(GPU_TEXUNIT1, &pglState->textures[pglState->textureBound[1]]);
		}
		else
			_picaTextureEnvSet(1, &pglState->texenv[PGL_TEXENV_DUMMY]);

		_picaTexUnitEnable(texunit_enable_mask);
	}

	if(pglState->changes & STATE_MATRIX_CHANGE)
	{
		matrix4x4_multiply(&matrix_mvp, &pglState->matrix_projection, &pglState->matrix_modelview);
		_picaUniformFloat(GPU_VERTEX_SHADER, 0, (float*)&matrix_mvp, 4);
	}

	pglState->changes = 0;
}

void glDisable(GLenum cap)
{
	switch (cap)
	{
		case GL_DEPTH_TEST:
			pglState->depthTestState = false;
			pglState->changes |= STATE_DEPTHTEST_CHANGE;
			break;
		case GL_STENCIL_TEST:
			pglState->stencilTestState = false;
			pglState->changes |= STATE_STENCIL_CHANGE;
			break;
		case GL_BLEND:
			pglState->blendState = false;
			pglState->changes |= STATE_BLEND_CHANGE;
			break;
		case GL_SCISSOR_TEST:
			break;
		case GL_CULL_FACE:
			pglState->cullState = false;
			pglState->changes |= STATE_CULL_CHANGE;
			break;
		case GL_TEXTURE_2D:
			pglState->texUnitState[pglState->texUnitActive] = false;
			pglState->changes |= STATE_TEXTURE_CHANGE;
			break;
		case GL_ALPHA_TEST:
			pglState->alphaTestState = false;
			pglState->changes |= STATE_ALPHATEST_CHANGE;
			break;
		default:
			break;
	}
}

void glEnable(GLenum cap)
{
	switch (cap)
	{
		case GL_DEPTH_TEST:
			pglState->depthTestState = true;
			pglState->changes |= STATE_DEPTHTEST_CHANGE;
			break;
		case GL_STENCIL_TEST:
			pglState->stencilTestState = true;
			pglState->changes |= STATE_STENCIL_CHANGE;
			break;
		case GL_BLEND:
			pglState->blendState = true;
			pglState->changes |= STATE_BLEND_CHANGE;
			break;
		case GL_SCISSOR_TEST:
			break;
		case GL_CULL_FACE:
			pglState->cullState = true;
			pglState->changes |= STATE_CULL_CHANGE;
			break;
		case GL_TEXTURE_2D:
			pglState->texUnitState[pglState->texUnitActive] = true;
			pglState->changes |= STATE_TEXTURE_CHANGE;
			break;
		case GL_ALPHA_TEST:
			pglState->alphaTestState = true;
			pglState->changes |= STATE_ALPHATEST_CHANGE;
			break;
		default:
			break;
	}
}