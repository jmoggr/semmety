#include "SemmetyLayout.hpp"
#include "SemmetyWorkspaceWrapper.hpp"
#include "log.hpp"
#include "src/globals.hpp"
#include "utils.hpp"

void SemmetyLayout::workspaceHook(void*, SCallbackInfo&, std::any data) {
	semmety_log(ERR, "WORKSPACE_HOOK");
	updateBar();
}

void SemmetyLayout::urgentHook(void*, SCallbackInfo&, std::any data) {
	auto layout = g_SemmetyLayout.get();
	if (layout == nullptr) {
		return;
	}

	// m_bIsUrgent is not set until after the hook event is handled, so we hack it this way
	layout->updateBarOnNextTick = true;
	g_pAnimationManager->scheduleTick();
}

void SemmetyLayout::renderHook(void*, SCallbackInfo&, std::any data) {
	static auto PBORDERSIZE = CConfigValue<Hyprlang::INT>("general:border_size");
	static auto PROUNDING = CConfigValue<Hyprlang::INT>("decoration:rounding");
	static auto PROUNDINGPOWER = CConfigValue<Hyprlang::FLOAT>("decoration:rounding_power");

	auto render_stage = std::any_cast<eRenderStage>(data);

	auto monitor = g_pHyprOpenGL->m_RenderData.pMonitor.lock();
	if (monitor == nullptr) {
		return;
	}

	if (monitor->activeWorkspace == nullptr) {
		return;
	}

	auto layout = g_SemmetyLayout.get();
	if (layout == nullptr) {
		return;
	}
	auto ww = layout->getOrCreateWorkspaceWrapper(monitor->activeWorkspace);
	auto emptyFrames = ww.root->getEmptyFrames();

	switch (render_stage) {
	case RENDER_PRE_WINDOWS:
		for (const auto& frame: emptyFrames) {
			CBorderPassElement::SBorderData borderData;
			borderData.box = frame->getEmptyFrameBox(*monitor);
			borderData.borderSize = *PBORDERSIZE;
			borderData.roundingPower = *PROUNDINGPOWER;
			borderData.round = *PROUNDING;

			auto grad = frame->m_cRealBorderColor;
			// angle animation it not enabled yet
			// if (frame.m_fBorderAngleAnimationProgress->enabled()) {
			//     grad.m_fAngle += frame.m_fBorderAngleAnimationProgress->value() * M_PI * 2;
			//     grad.m_fAngle = normalizeAngleRad(grad.m_fAngle);
			// }

			// TODO: what does a do?
			// data.a             = a;

			const bool ANIMATED = frame->m_fBorderFadeAnimationProgress->isBeingAnimated();
			if (ANIMATED) {
				borderData.hasGrad2 = true;
				borderData.grad1 = frame->m_cRealBorderColorPrevious;
				borderData.grad2 = grad;
				borderData.lerp = frame->m_fBorderFadeAnimationProgress->value();
			} else {
				borderData.grad1 = grad;
			}

			auto element = CBorderPassElement(borderData);
			auto pass = makeShared<CBorderPassElement>(element);
			g_pHyprRenderer->m_sRenderPass.add(pass);
		}

		break;
	default: break;
	}
}

void SemmetyLayout::tickHook(void*, SCallbackInfo&, std::any) {
	auto layout = g_SemmetyLayout.get();
	if (layout == nullptr) {
		return;
	}

	if (layout->updateBarOnNextTick) {
		updateBar();
		layout->updateBarOnNextTick = false;
	}

	if (g_pLayoutManager->getCurrentLayout() != layout) return;

	for (const auto& monitor: g_pCompositor->m_vMonitors) {
		if (monitor->activeWorkspace == nullptr) {
			continue;
		}

		const auto activeWorkspace = monitor->activeWorkspace;
		if (activeWorkspace == nullptr) {
			continue;
		}

		const auto ww = layout->getOrCreateWorkspaceWrapper(monitor->activeWorkspace);
		auto emptyFrames = ww.root->getEmptyFrames();

		for (const auto& frame: emptyFrames) {
			frame->damageEmptyFrameBox(*monitor);
		}
	}
}
