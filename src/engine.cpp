#include "engine.hpp"

#include "window.hpp"
#include "gfx_device.hpp"
#include "resource_manager.hpp"
#include "log.hpp"

#include <glm/glm.hpp>

static engine::gfx::VertexBuffer* buffer;

namespace engine {

	Application::Application(const char* appName, const char* appVersion)
	{
		m_win = std::make_unique<Window>(appName, true);
		m_gfx = std::make_unique<GFXDevice>(appName, appVersion, m_win->getHandle());

		engine::ResourceManager resMan{};

		struct Vertex {
			glm::vec2 pos;
			glm::vec3 col;
		};

		gfx::VertexFormat vertFormat{
			.stride = (uint32_t)sizeof(Vertex),
		};

		vertFormat.attributeDescriptions.push_back({0, gfx::VertexAttribFormat::VEC2, 0});
		vertFormat.attributeDescriptions.push_back({1, gfx::VertexAttribFormat::VEC3, offsetof(Vertex, col)});

		m_gfx->createPipeline(resMan.getFilePath("shader.vert.spv").string().c_str(), resMan.getFilePath("shader.frag.spv").string().c_str(), vertFormat);



		const std::vector<Vertex> vertices = {
			{	{ 0.0f,	-0.5f},	{1.0f, 0.0f, 0.0f}	},
			{	{ 0.5f,	 0.5f},	{0.0f, 1.0f, 0.0f}	},
			{	{-0.5f,	 0.5f},	{0.0f, 0.0f, 1.0f}	}
		};
		const std::vector<uint16_t> indices{
			0, 1, 2,
		};
		buffer = m_gfx->createVertexBuffer(sizeof(Vertex) * vertices.size(), vertices.data(), indices.data());

		const std::vector<Vertex> vertices2 = {
			{	{ 0.9f,	-0.9f},	{1.0f, 0.0f, 0.0f}	},
			{	{ 0.9f,	-0.8f},	{1.0f, 0.0f, 0.0f}	},
			{	{ 0.8f,	-0.9f},	{1.0f, 0.0f, 0.0f}	}
		};
		
	}

	Application::~Application()
	{
		m_gfx->destroyVertexBuffer(buffer);
	}

	void Application::gameLoop()
	{
		TRACE("Begin game loop...");

		uint64_t lastTick = m_win->getNanos();
		constexpr int TICKFREQ = 1; // in hz

		// single-threaded game loop
		while (m_win->isRunning()) {

			/* logic */

			if (m_win->getLastFrameStamp() >= lastTick + (BILLION / TICKFREQ)) {
				lastTick = m_win->getLastFrameStamp();

				// do tick stuff here
				m_win->setTitle("frame time: " + std::to_string(m_win->dt() * 1000.0f) + " ms, " + std::to_string(m_win->getAvgFPS()) + " fps");
				m_win->resetAvgFPS();
			}

			if (m_win->getKeyPress(inputs::Key::F11)) {
				m_win->toggleFullscreen();
			}
			if (m_win->getKeyPress(inputs::Key::ESCAPE)) {
				m_win->setCloseFlag();
			}

			/* draw */

			m_gfx->drawBuffer(buffer);

			m_gfx->draw();

			/* poll events */
			m_win->getInputAndEvents();

		}

		m_gfx->waitIdle();
	}

}
