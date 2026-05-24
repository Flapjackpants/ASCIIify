#include "ASCIIifyPlugin.h"

#include "../core/ascii_core.hpp"
#include "../core/image_ops.hpp"

#include "ofxsImageEffect.h"

#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

#define kPluginName "ASCIIify"
#define kPluginGrouping "Filter"
#define kPluginDescription "Convert video frames to colored ASCII art"
#define kPluginIdentifier "com.maxwellli.ASCIIify"
#define kPluginVersionMajor 1
#define kPluginVersionMinor 0

namespace {

std::string bundledFontPath() {
    const std::filesystem::path candidates[] = {
        "/Library/OFX/Plugins/ASCIIify.ofx.bundle/Contents/Resources/AndaleMono.ttf",
        "ASCIIify.ofx.bundle/Contents/Resources/AndaleMono.ttf",
        "../ASCIIify.ofx.bundle/Contents/Resources/AndaleMono.ttf",
        "../../build/ASCIIify.ofx.bundle/Contents/Resources/AndaleMono.ttf",
    };
    for (const auto& p : candidates) {
        if (std::filesystem::exists(p)) {
            return p.string();
        }
    }
    return defaultFontPath();
}

AsciiOptions optionsFromParams(OFX::ImageEffect& effect, double time) {
    AsciiOptions opts;

    opts.cols = effect.fetchIntParam("cols")->getValueAtTime(time);
    const bool auto_rows = effect.fetchBooleanParam("autoRows")->getValueAtTime(time);
    opts.rows = auto_rows ? 0 : effect.fetchIntParam("rows")->getValueAtTime(time);
    opts.font_scale = effect.fetchDoubleParam("fontScale")->getValueAtTime(time);
    opts.use_color = effect.fetchBooleanParam("useColor")->getValueAtTime(time);
    opts.invert = effect.fetchBooleanParam("invert")->getValueAtTime(time);
    opts.saturation_boost = effect.fetchDoubleParam("saturation")->getValueAtTime(time);
    opts.color_luma_scale = effect.fetchDoubleParam("brightness")->getValueAtTime(time);
    opts.mono_luma_scale = effect.fetchDoubleParam("monoDarken")->getValueAtTime(time);
    effect.fetchStringParam("charRamp")->getValueAtTime(time, opts.char_ramp);
    opts.font_path = bundledFontPath();

    if (opts.char_ramp.empty()) {
        opts.char_ramp = kDefaultCharRamp;
    }
    return opts;
}

} // namespace

class ASCIIifyPlugin : public OFX::ImageEffect
{
public:
    explicit ASCIIifyPlugin(OfxImageEffectHandle p_Handle);

    virtual void render(const OFX::RenderArguments& p_Args) override;
    virtual bool isIdentity(const OFX::IsIdentityArguments& p_Args, OFX::Clip*& p_IdentityClip,
                            double& p_IdentityTime) override;
    virtual void changedParam(const OFX::InstanceChangedArgs& p_Args, const std::string& p_ParamName) override;

private:
    void setRowsEnabledness();

    OFX::Clip* m_DstClip;
    OFX::Clip* m_SrcClip;

    OFX::BooleanParam* m_Bypass;
    OFX::IntParam* m_Cols;
    OFX::BooleanParam* m_AutoRows;
    OFX::IntParam* m_Rows;
    OFX::DoubleParam* m_FontScale;
    OFX::BooleanParam* m_UseColor;
    OFX::BooleanParam* m_Invert;
    OFX::DoubleParam* m_Saturation;
    OFX::DoubleParam* m_Brightness;
    OFX::DoubleParam* m_MonoDarken;
    OFX::StringParam* m_CharRamp;
};

ASCIIifyPlugin::ASCIIifyPlugin(OfxImageEffectHandle p_Handle)
    : ImageEffect(p_Handle)
{
    m_DstClip = fetchClip(kOfxImageEffectOutputClipName);
    m_SrcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);

    m_Bypass = fetchBooleanParam("bypass");
    m_Cols = fetchIntParam("cols");
    m_AutoRows = fetchBooleanParam("autoRows");
    m_Rows = fetchIntParam("rows");
    m_FontScale = fetchDoubleParam("fontScale");
    m_UseColor = fetchBooleanParam("useColor");
    m_Invert = fetchBooleanParam("invert");
    m_Saturation = fetchDoubleParam("saturation");
    m_Brightness = fetchDoubleParam("brightness");
    m_MonoDarken = fetchDoubleParam("monoDarken");
    m_CharRamp = fetchStringParam("charRamp");

    setRowsEnabledness();
}

void ASCIIifyPlugin::setRowsEnabledness() {
    const bool auto_rows = m_AutoRows->getValue();
    m_Rows->setEnabled(!auto_rows);
}

void ASCIIifyPlugin::changedParam(const OFX::InstanceChangedArgs& /*p_Args*/,
                                  const std::string& p_ParamName) {
    if (p_ParamName == "autoRows") {
        setRowsEnabledness();
    }
}

bool ASCIIifyPlugin::isIdentity(const OFX::IsIdentityArguments& p_Args, OFX::Clip*& p_IdentityClip,
                                double& p_IdentityTime) {
    if (m_Bypass->getValueAtTime(p_Args.time)) {
        p_IdentityClip = m_SrcClip;
        p_IdentityTime = p_Args.time;
        return true;
    }
    return false;
}

void ASCIIifyPlugin::render(const OFX::RenderArguments& p_Args) {
    if (m_DstClip->getPixelDepth() != OFX::eBitDepthFloat ||
        m_DstClip->getPixelComponents() != OFX::ePixelComponentRGBA) {
        OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
    }

    std::unique_ptr<OFX::Image> dst(m_DstClip->fetchImage(p_Args.time));
    std::unique_ptr<OFX::Image> src(m_SrcClip->fetchImage(p_Args.time));

    const OfxRectI& bounds = src->getBounds();
    const int width = bounds.x2 - bounds.x1;
    const int height = bounds.y2 - bounds.y1;

    FloatImageBuffer src_buf(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float* sp = static_cast<float*>(src->getPixelAddress(bounds.x1 + x, bounds.y1 + y));
            float* dp = src_buf.pixel(x, y);
            if (sp) {
                dp[0] = sp[0];
                dp[1] = sp[1];
                dp[2] = sp[2];
                dp[3] = sp[3];
            } else {
                dp[0] = dp[1] = dp[2] = dp[3] = 0.f;
            }
        }
    }

    AsciiOptions opts = optionsFromParams(*this, p_Args.time);
    AsciiCore core(opts);

    FloatImageBuffer out_buf(width, height);
    core.convertFloatRgbaToSize(src_buf, out_buf);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float* dp = static_cast<float*>(dst->getPixelAddress(bounds.x1 + x, bounds.y1 + y));
            const float* sp = out_buf.pixel(x, y);
            if (dp) {
                dp[0] = sp[0];
                dp[1] = sp[1];
                dp[2] = sp[2];
                dp[3] = sp[3];
            }
        }
    }
}

using namespace OFX;

ASCIIifyPluginFactory::ASCIIifyPluginFactory()
    : OFX::PluginFactoryHelper<ASCIIifyPluginFactory>(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor)
{
}

void ASCIIifyPluginFactory::describe(OFX::ImageEffectDescriptor& p_Desc) {
    p_Desc.setLabels(kPluginName, kPluginName, kPluginName);
    p_Desc.setPluginGrouping(kPluginGrouping);
    p_Desc.setPluginDescription(kPluginDescription);

    p_Desc.addSupportedContext(eContextFilter);
    p_Desc.addSupportedContext(eContextGeneral);
    p_Desc.addSupportedBitDepth(eBitDepthFloat);

    p_Desc.setSingleInstance(false);
    p_Desc.setHostFrameThreading(false);
    p_Desc.setSupportsMultiResolution(false);
    p_Desc.setSupportsTiles(false);
    p_Desc.setTemporalClipAccess(false);
    p_Desc.setRenderTwiceAlways(false);
    p_Desc.setSupportsMultipleClipPARs(false);
    p_Desc.setNoSpatialAwareness(true);

    p_Desc.setSupportsOpenCLRender(false);
#ifndef __APPLE__
    p_Desc.setSupportsCudaRender(false);
#endif
#ifdef __APPLE__
    p_Desc.setSupportsMetalRender(false);
#endif
}

void ASCIIifyPluginFactory::describeInContext(OFX::ImageEffectDescriptor& p_Desc, OFX::ContextEnum /*p_Context*/) {
    ClipDescriptor* srcClip = p_Desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(false);
    srcClip->setIsMask(false);

    ClipDescriptor* dstClip = p_Desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->setSupportsTiles(false);

    PageParamDescriptor* page = p_Desc.definePageParam("Controls");

    BooleanParamDescriptor* bypass = p_Desc.defineBooleanParam("bypass");
    bypass->setLabels("Bypass", "Bypass", "Bypass");
    bypass->setDefault(false);
    page->addChild(*bypass);

    GroupParamDescriptor* gridGroup = p_Desc.defineGroupParam("gridGroup");
    gridGroup->setLabels("Grid", "Grid", "Grid");

    IntParamDescriptor* cols = p_Desc.defineIntParam("cols");
    cols->setLabels("Columns", "Columns", "Columns");
    cols->setDefault(120);
    cols->setRange(20, 300);
    cols->setParent(*gridGroup);
    page->addChild(*cols);

    BooleanParamDescriptor* autoRows = p_Desc.defineBooleanParam("autoRows");
    autoRows->setLabels("Auto Rows", "Auto Rows", "Auto Rows");
    autoRows->setDefault(true);
    autoRows->setParent(*gridGroup);
    page->addChild(*autoRows);

    IntParamDescriptor* rows = p_Desc.defineIntParam("rows");
    rows->setLabels("Rows", "Rows", "Rows");
    rows->setDefault(60);
    rows->setRange(1, 500);
    rows->setParent(*gridGroup);
    page->addChild(*rows);

    DoubleParamDescriptor* fontScale = p_Desc.defineDoubleParam("fontScale");
    fontScale->setLabels("Font Scale", "Font Scale", "Font Scale");
    fontScale->setDefault(0.4);
    fontScale->setRange(0.1, 2.0);
    fontScale->setDisplayRange(0.1, 2.0);
    fontScale->setParent(*gridGroup);
    page->addChild(*fontScale);

    GroupParamDescriptor* colorGroup = p_Desc.defineGroupParam("colorGroup");
    colorGroup->setLabels("Color", "Color", "Color");

    BooleanParamDescriptor* useColor = p_Desc.defineBooleanParam("useColor");
    useColor->setLabels("Use Color", "Use Color", "Use Color");
    useColor->setDefault(true);
    useColor->setParent(*colorGroup);
    page->addChild(*useColor);

    BooleanParamDescriptor* invert = p_Desc.defineBooleanParam("invert");
    invert->setLabels("Invert", "Invert", "Invert");
    invert->setDefault(false);
    invert->setParent(*colorGroup);
    page->addChild(*invert);

    DoubleParamDescriptor* saturation = p_Desc.defineDoubleParam("saturation");
    saturation->setLabels("Saturation", "Saturation", "Saturation");
    saturation->setDefault(1.0);
    saturation->setRange(0.0, 3.0);
    saturation->setDisplayRange(0.0, 3.0);
    saturation->setParent(*colorGroup);
    page->addChild(*saturation);

    DoubleParamDescriptor* brightness = p_Desc.defineDoubleParam("brightness");
    brightness->setLabels("Brightness", "Brightness", "Brightness");
    brightness->setDefault(1.0);
    brightness->setRange(0.1, 3.0);
    brightness->setDisplayRange(0.1, 3.0);
    brightness->setParent(*colorGroup);
    page->addChild(*brightness);

    DoubleParamDescriptor* monoDarken = p_Desc.defineDoubleParam("monoDarken");
    monoDarken->setLabels("Mono Darken", "Mono Darken", "Mono Darken");
    monoDarken->setDefault(0.5);
    monoDarken->setRange(0.1, 2.0);
    monoDarken->setDisplayRange(0.1, 2.0);
    monoDarken->setParent(*colorGroup);
    page->addChild(*monoDarken);

    GroupParamDescriptor* charGroup = p_Desc.defineGroupParam("charGroup");
    charGroup->setLabels("Characters", "Characters", "Characters");

    StringParamDescriptor* charRamp = p_Desc.defineStringParam("charRamp");
    charRamp->setLabels("Character Ramp", "Character Ramp", "Character Ramp");
    charRamp->setDefault(kDefaultCharRamp);
    charRamp->setStringType(eStringTypeSingleLine);
    charRamp->setParent(*charGroup);
    page->addChild(*charRamp);
}

ImageEffect* ASCIIifyPluginFactory::createInstance(OfxImageEffectHandle p_Handle, ContextEnum /*p_Context*/) {
    return new ASCIIifyPlugin(p_Handle);
}

void OFX::Plugin::getPluginIDs(PluginFactoryArray& p_FactoryArray) {
    static ASCIIifyPluginFactory factory;
    p_FactoryArray.push_back(&factory);
}
