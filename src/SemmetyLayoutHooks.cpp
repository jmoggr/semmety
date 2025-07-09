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
	auto render_stage = std::any_cast<eRenderStage>(data);

	static auto PACTIVECOL = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.active_border");
	static auto PINACTIVECOL = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.inactive_border");
	static auto PBORDERSIZE = CConfigValue<Hyprlang::INT>("general:border_size");

	static auto PROUNDING = CConfigValue<Hyprlang::INT>("decoration:rounding");
	static auto PROUNDINGPOWER = CConfigValue<Hyprlang::FLOAT>("decoration:rounding_power");

	auto* const ACTIVECOL = (CGradientValueData*) (PACTIVECOL.ptr())->getData();
	auto* const INACTIVECOL = (CGradientValueData*) (PINACTIVECOL.ptr())->getData();

	auto monitor = g_pHyprOpenGL->m_renderData.pMonitor.lock();
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
			if (ww.getFocusedFrame() == frame) {
				borderData.grad1 = *ACTIVECOL;
			} else {
				borderData.grad1 = *INACTIVECOL;
			}
			borderData.box = frame->getEmptyFrameBox(*monitor);

			borderData.borderSize = *PBORDERSIZE;
			borderData.roundingPower = *PROUNDINGPOWER;
			borderData.round = *PROUNDING;
			auto element = CBorderPassElement(borderData);
			auto pass = makeShared<CBorderPassElement>(element);
			g_pHyprRenderer->m_renderPass.add(pass);
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

void SemmetyLayout::activeWindowHook(void*, SCallbackInfo&, std::any data) {
	semmety_log(ERR, "activate hook");
	const auto PWINDOW = std::any_cast<PHLWINDOW>(data);
	if (PWINDOW == nullptr) {
		return;
	}

	if (PWINDOW->m_workspace == nullptr) {
		return;
	}

	g_SemmetyLayout->activateWindow(PWINDOW);
}
