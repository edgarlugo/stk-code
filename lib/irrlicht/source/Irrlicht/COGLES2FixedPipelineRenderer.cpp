// Copyright (C) 2013 Patryk Nadrowski
// Heavily based on the OpenGL driver implemented by Nikolaus Gebhardt
// OpenGL ES driver implemented by Christian Stehno and first OpenGL ES 2.0
// driver implemented by Amundis.
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#include "IrrCompileConfig.h"

#ifdef _IRR_COMPILE_WITH_OGLES2_

#include "COGLES2FixedPipelineRenderer.h"
#include "IGPUProgrammingServices.h"
#include "os.h"
#include "COGLES2Driver.h"

#include "ge_render_info.hpp"

namespace irr
{
namespace video
{

//! Constructor
COGLES2FixedPipelineRenderer::COGLES2FixedPipelineRenderer(const c8* vertexShaderProgram,
							const c8* pixelShaderProgram, E_MATERIAL_TYPE baseMaterial,
							COGLES2Driver* driver, bool variant)
	: COGLES2MaterialRenderer(driver, 0, baseMaterial)
{
	#ifdef _DEBUG
	setDebugName("COGLES2FixedPipelineRenderer");
	#endif
	yy = (int)baseMaterial;
	int Temp = 0;

#ifdef __vita__
	// The variants are built when first used
	SharedRenderer = 0;
	for (int i = 0; i < 4; i++)
		Variants[i] = 0;
	if (variant)
		init(Temp, vertexShaderProgram, pixelShaderProgram, false);
	else
	{
		VertexShader = vertexShaderProgram;
		PixelShader = pixelShaderProgram;
	}
#else
	SharedRenderer = reinterpret_cast<COGLES2MaterialRenderer*>(driver->getMaterialRenderer(EMT_SOLID));

	if (SharedRenderer)
		SharedRenderer->grab();
	else
		init(Temp, vertexShaderProgram, pixelShaderProgram, false);
#endif

	HueChangeID = getPixelShaderConstantID("uHueChange");
	VertexColorID = getPixelShaderConstantID("uVertexColor");
	MaterialTypeID = getPixelShaderConstantID("uMaterialType");
	WorldMatrixID = getPixelShaderConstantID("uWorldMatrix");
	MvpMatrixID = getPixelShaderConstantID("uMvpMatrix");
	TextureUsage0ID = getPixelShaderConstantID("uTextureUsage0");
	TextureTrans0ID = getVertexShaderConstantID("uTextureTrans0");
	TextureUnit0ID = getPixelShaderConstantID("uTextureUnit0");
}


//! Destructor
COGLES2FixedPipelineRenderer::~COGLES2FixedPipelineRenderer()
{
	if(SharedRenderer)
		SharedRenderer->drop();
#ifdef __vita__
	for (int i = 0; i < 4; i++)
	{
		if (Variants[i])
			Variants[i]->drop();
	}
#endif
}


static s32 getMaterialTypeNr(E_MATERIAL_TYPE type, bool& second_texture)
{
	second_texture = false;
	switch(type)
	{
	case EMT_SOLID_2_LAYER:
		return 1;
	case EMT_LIGHTMAP:
	case EMT_LIGHTMAP_ADD:
	case EMT_LIGHTMAP_M2:
	case EMT_LIGHTMAP_M4:
		return 2;
	case EMT_LIGHTMAP_LIGHTING:
	case EMT_LIGHTMAP_LIGHTING_M2:
	case EMT_LIGHTMAP_LIGHTING_M4:
		second_texture = true;
		return 2;
	case EMT_DETAIL_MAP:
		return 3;
	case EMT_SPHERE_MAP:
		return 4;
	case EMT_REFLECTION_2_LAYER:
		return 5;
	case EMT_TRANSPARENT_ALPHA_CHANNEL:
		return 6;
	case EMT_TRANSPARENT_ALPHA_CHANNEL_REF:
		return 7;
	case EMT_TRANSPARENT_VERTEX_ALPHA:
		return 8;
	case EMT_TRANSPARENT_REFLECTION_2_LAYER:
		return 9;
	case EMT_STK_GRASS:
		return 10;
	case EMT_ONETEXTURE_BLEND:
		return 11;
	default:
		return 0;
	}
}


#ifdef __vita__
COGLES2FixedPipelineRenderer* COGLES2FixedPipelineRenderer::getVariant(const SMaterial& material)
{
	const bool texture = material.getTexture(0) != 0;
	const bool hue = material.getRenderInfo() && material.getRenderInfo()->getHue() > 0.0f;
	COGLES2FixedPipelineRenderer*& variant = Variants[hue * 2 + texture];
	if (!variant)
	{
		bool second_texture;
		core::stringc pixel_shader = "#define MATERIAL_TYPE ";
		pixel_shader += core::stringc(getMaterialTypeNr((E_MATERIAL_TYPE)yy, second_texture));
		pixel_shader += texture ? "\n#define TEXTURE_USAGE true\n" : "\n#define TEXTURE_USAGE false\n";
		pixel_shader += hue ? "#define HUE_CHANGE true\n" : "#define HUE_CHANGE false\n";
		pixel_shader += PixelShader;
		variant = new COGLES2FixedPipelineRenderer(VertexShader.c_str(), pixel_shader.c_str(),
			(E_MATERIAL_TYPE)yy, Driver, true);
	}
	return variant;
}
#endif


void COGLES2FixedPipelineRenderer::OnSetMaterial(const video::SMaterial& material,
				const video::SMaterial& lastMaterial,
				bool resetAllRenderstates,
				video::IMaterialRendererServices* services)
{
	if (SharedRenderer)
		Driver->getBridgeCalls()->setProgram(SharedRenderer->getProgram());
#ifdef __vita__
	else if (VertexShader.size())
		Driver->getBridgeCalls()->setProgram(getVariant(material)->getProgram());
#endif
	else
		Driver->getBridgeCalls()->setProgram(Program);

	Driver->setBasicRenderStates(material, lastMaterial, resetAllRenderstates);

	if (FixedBlending)
	{
		Driver->getBridgeCalls()->setBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
		Driver->getBridgeCalls()->setBlend(true);
	}
	else if (Blending)
	{
		E_BLEND_FACTOR srcFact,dstFact;
		E_MODULATE_FUNC modulate;
		u32 alphaSource;
		unpack_textureBlendFunc(srcFact, dstFact, modulate, alphaSource, material.MaterialTypeParam);

		Driver->getBridgeCalls()->setBlendFunc(Driver->getGLBlend(srcFact), Driver->getGLBlend(dstFact));
		Driver->getBridgeCalls()->setBlend(true);
	}
	else
		Driver->getBridgeCalls()->setBlend(false);
}


bool COGLES2FixedPipelineRenderer::OnRender(IMaterialRendererServices* service, E_VERTEX_TYPE vtxtype)
{
	if (SharedRenderer)
		return SharedRenderer->OnRender(service, vtxtype);
#ifdef __vita__
	else if (VertexShader.size())
		return getVariant(Driver->getCurrentMaterial())->OnRender(service, vtxtype);
#endif
	else
	{
		Driver->setTextureRenderStates(Driver->getCurrentMaterial(), false);

		bool second_texture;
		s32 materialType = getMaterialTypeNr(Driver->getCurrentMaterial().MaterialType, second_texture);

		video::SColorf vertex_color(SColor(-1));
		float hue_change = 0.0f;
		if (Driver->getCurrentMaterial().getRenderInfo())
		{
			hue_change = Driver->getCurrentMaterial().getRenderInfo()->getHue();
			vertex_color = video::SColorf(Driver->getCurrentMaterial().getRenderInfo()->getVertexColor());
		}
		setPixelShaderConstant(HueChangeID, &hue_change, 1);
		setPixelShaderConstant(VertexColorID, &vertex_color.r, 4);

		setPixelShaderConstant(MaterialTypeID, &materialType, 1);

		/* Transform Matrices Upload */

		core::matrix4 world = Driver->getTransform(ETS_WORLD);
		setPixelShaderConstant(WorldMatrixID, world.pointer(), 16);

		core::matrix4 worldViewProj = Driver->getTransform(video::ETS_PROJECTION);
		worldViewProj *= Driver->getTransform(video::ETS_VIEW);
		worldViewProj *= Driver->getTransform(ETS_WORLD);
		setPixelShaderConstant(MvpMatrixID, worldViewProj.pointer(), 16);

		/* Textures Upload */

		s32 TextureUsage0 = Driver->isActiveTexture(0);
		//s32 TextureUsage1 = Driver->isActiveTexture(1);

		setPixelShaderConstant(TextureUsage0ID, &TextureUsage0, 1);
		//IMaterialRendererServices::setPixelShaderConstant("uTextureUsage1", &TextureUsage1, 1);

		const irr::core::matrix4& textureMatrix0 = Driver->getCurrentMaterial().getTextureMatrix(0);
		//core::matrix4 textureMatrix1 = Driver->getTransform(video::ETS_TEXTURE_0);

		setVertexShaderConstant(TextureTrans0ID, &textureMatrix0[8], 2);
		//IMaterialRendererServices::setPixelShaderConstant("uTextureMatrix1", textureMatrix1.pointer(), 16);

		s32 TextureUnit0 = 0;
		//s32 TextureUnit1 = 1;
		
		if (second_texture)
			TextureUnit0 = 1;

		setPixelShaderConstant(TextureUnit0ID, &TextureUnit0, 1);
		//IMaterialRendererServices::setPixelShaderConstant("uTextureUnit1", &TextureUnit1, 1);

		return true;
	}
}


} // end namespace video
} // end namespace irr


#endif
