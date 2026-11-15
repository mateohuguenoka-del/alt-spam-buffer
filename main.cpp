// Alt-Spam Input Buffer
//
// Problem: when you alternate-spam (two keys/fingers), presses and releases
// overlap or land mid-air, so jumps get dropped or fire at the wrong place.
//
// What this does:
//  1. Counts held keys per player, so releasing key A while key B is still
//     down does NOT send a bogus release ("ghost release").
//  2. Remembers a click made while airborne and replays it on the exact
//     physics step the player lands -> the click lands "on the right spot".
//  3. Draws a translucent ghost player where the buffered click was queued.
//
// Orbs/pads still get the live press (the press is passed through to the game
// immediately); the buffer only adds a replay on landing.

#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <vector>

using namespace geode::prelude;

namespace {
	struct Pending {
		bool p1 = true;
		float age = 0.f;
		bool released = false;
		Ref<SimplePlayer> ghost;
	};
	struct Release {
		bool p1 = true;
		float timeLeft = 0.f;
	};
	struct State {
		std::vector<Pending> pending;
		std::vector<Release> releases;
		int held[2] = {0, 0};
		bool flushing = false;

		void clear() {
			for (auto& p : pending) {
				if (p.ghost) p.ghost->removeFromParent();
			}
			pending.clear();
			releases.clear();
			held[0] = held[1] = 0;
			flushing = false;
		}
	};
	State& S() {
		static State s;
		return s;
	}

	bool on() { return Mod::get()->getSettingValue<bool>("enabled"); }

	// Flying modes don't need landing buffering.
	bool bufferable(PlayerObject* p) {
		return p && !p->m_isShip && !p->m_isBird && !p->m_isDart && !p->m_isSwing;
	}

	void queueClick(GJBaseGameLayer* gl, PlayerObject* p, bool isP1) {
		auto& s = S();
		Pending* slot = nullptr;
		for (auto& pd : s.pending) {
			if (pd.p1 == isP1) { slot = &pd; break; }
		}
		if (!slot) {
			s.pending.emplace_back();
			slot = &s.pending.back();
			slot->p1 = isP1;
		}
		// newest press wins: restart the window
		slot->age = 0.f;
		slot->released = false;

		if (Mod::get()->getSettingValue<bool>("show-ghosts") && gl->m_objectLayer) {
			if (!slot->ghost) {
				auto gm = GameManager::get();
				auto g = SimplePlayer::create(gm->getPlayerFrame());
				g->setColor(gm->colorForIdx(gm->getPlayerColor()));
				g->setSecondColor(gm->colorForIdx(gm->getPlayerColor2()));
				g->setZOrder(1000);
				gl->m_objectLayer->addChild(g);
				slot->ghost = g;
			}
			slot->ghost->setPosition(p->getPosition());
			slot->ghost->setRotation(p->getRotation());
			slot->ghost->setOpacity(static_cast<GLubyte>(
				Mod::get()->getSettingValue<int64_t>("ghost-opacity")));
		}
	}

	void tick(GJBaseGameLayer* gl, float dt) {
		auto& s = S();
		if (!on()) {
			if (!s.pending.empty() || !s.releases.empty()) s.clear();
			return;
		}
		float window = Mod::get()->getSettingValue<int64_t>("buffer-ms") / 1000.f;
		s.flushing = true;

		// delayed releases for replayed clicks
		for (auto it = s.releases.begin(); it != s.releases.end();) {
			it->timeLeft -= dt;
			if (it->timeLeft <= 0.f) {
				gl->handleButton(false, 1, it->p1);
				it = s.releases.erase(it);
			} else ++it;
		}

		// buffered clicks
		for (auto it = s.pending.begin(); it != s.pending.end();) {
			auto p = it->p1 ? gl->m_player1 : gl->m_player2;
			it->age += dt;
			bool done = false;

			if (!p || p->m_isDead || !bufferable(p)) {
				done = true;
			} else if (p->m_isOnGround) {
				done = true;
				// Player let go in the air -> replay the click on this landing step.
				// (If still holding, the game's own hold-to-jump already handles it.)
				if (it->released) {
					gl->handleButton(true, 1, it->p1);
					s.releases.push_back({it->p1, 0.006f});
				}
			} else if (it->age > window) {
				done = true;
			}

			if (done) {
				if (it->ghost) it->ghost->removeFromParent();
				it = s.pending.erase(it);
			} else ++it;
		}
		s.flushing = false;
	}
}

class $modify(AltSpamGameLayer, GJBaseGameLayer) {
	void handleButton(bool down, int button, bool isPlayer1) {
		auto& s = S();
		if (s.flushing || button != 1 || !on()) {
			GJBaseGameLayer::handleButton(down, button, isPlayer1);
			return;
		}

		int i = isPlayer1 ? 0 : 1;
		auto p = isPlayer1 ? m_player1 : m_player2;

		if (down) {
			bool already = s.held[i]++ > 0;
			if (!already) {
				GJBaseGameLayer::handleButton(true, button, isPlayer1);
			} else if (Mod::get()->getSettingValue<bool>("alt-rehit")
				&& p && p->m_isOnGround && bufferable(p)) {
				// 2nd finger while 1st is down: make a fresh click
				GJBaseGameLayer::handleButton(false, button, isPlayer1);
				GJBaseGameLayer::handleButton(true, button, isPlayer1);
			}
			if (p && !p->m_isOnGround && bufferable(p)) {
				queueClick(this, p, isPlayer1);
			}
			return;
		}

		// release
		if (s.held[i] > 0) s.held[i]--;
		if (s.held[i] > 0) return; // other key still down: swallow the ghost release
		for (auto& pd : s.pending) {
			if (pd.p1 == isPlayer1) pd.released = true;
		}
		GJBaseGameLayer::handleButton(false, button, isPlayer1);
	}

	// NOTE: if this signature fails to compile on your Geode/GD version,
	// check Bindings (GJBaseGameLayer::processCommands) and match it.
	void processCommands(float dt, bool halfTick, bool lastTick) {
		tick(this, dt);
		GJBaseGameLayer::processCommands(dt, halfTick, lastTick);
	}
};

class $modify(AltSpamPlayLayer, PlayLayer) {
	void resetLevel() {
		S().clear();
		PlayLayer::resetLevel();
	}
	void onQuit() {
		S().clear();
		PlayLayer::onQuit();
	}
};
