#include "HudRenderer.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "../util/Logger.h"

namespace {

std::string formatTime(int ms) {
    if (ms < 0) {
        ms = 0;
    }
    const int totalSeconds = ms / 1000;
    const int minutes = totalSeconds / 60;
    const int seconds = totalSeconds % 60;
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << minutes << ":" << std::setw(2) << seconds;
    return oss.str();
}

} // namespace

HudRenderer::HudRenderer(SDL_Renderer* renderer, int height, int scale)
    : m_renderer(renderer), m_height(height), m_scale(scale), m_font(renderer) {
    LOG_T("HudRenderer ctor height=%d scale=%d", height, scale);
}

void HudRenderer::draw(const Game& game, int windowWidth) const {
    LOG_T("windowWidth:%d",windowWidth);

    if (!m_renderer) {
        return;
    }

    SDL_Rect background{ 0, 0, windowWidth, m_height };
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 200);
    SDL_RenderFillRect(m_renderer, &background);

    const SDL_Color primary{ 255, 255, 255, 255 };
    const SDL_Color warning{ 255, 165, 0, 255 };
    const SDL_Color danger{ 255, 64, 64, 255 };
    const SDL_Color success{ 0, 255, 127, 255 };

    const std::string levelLine =
        "NIVEAU " + std::to_string(game.currentLevelNumber()) + "/" +
        std::to_string(game.levelCount()) + " SCORE " + std::to_string(game.totalScore()) +
        " VIES " + std::to_string(game.remainingLives());
    m_font.drawText(8, 6, levelLine, primary, m_scale);

    const std::string diamondsText =
        "DIAMANTS " + std::to_string(game.collectedDiamonds()) + "/" +
        std::to_string(game.totalDiamonds());
    SDL_Color statusColor = primary;
    std::string statusText;
    if (game.exitUnlocked()) {
        statusColor = success;
        statusText = game.exitReached() ? "NIVEAU TERMINE" : "SORTIE OUVERTE";
    } else {
        const int remaining = std::max(0, game.totalDiamonds() - game.collectedDiamonds());
        statusColor = warning;
        statusText = "RESTE " + std::to_string(remaining);
    }
    m_font.drawText(8, 26, diamondsText, statusColor, m_scale);
    m_font.drawText(8, 46, statusText, statusColor, m_scale);

    const int remainingMs = game.timeRemainingMs();
    SDL_Color timerColor = primary;
    if (!game.levelComplete() && remainingMs > 0 &&
        remainingMs <= Game::TimeWarningThresholdMs) {
        const bool dangerPhase = (remainingMs / 500) % 2 == 0;
        timerColor = dangerPhase ? danger : warning;
    } else if (game.levelComplete()) {
        timerColor = success;
    }
    const std::string timerText = "TEMPS " + formatTime(remainingMs);
    const int timerWidth = m_font.textWidth(timerText, m_scale);
    m_font.drawText(std::max(8, windowWidth - timerWidth - 8), 6, timerText, timerColor, m_scale);
}
