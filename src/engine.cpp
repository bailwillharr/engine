#include "engine.hpp"

#include "window.hpp"
#include "gfx_device.hpp"
#include "resource_manager.hpp"
#include "log.hpp"

#include <glm/glm.hpp>

static engine::gfx::Pipeline* pipeline;
static engine::gfx::Buffer* vb;
static engine::gfx::Buffer* ib;
static engine::gfx::Buffer* ub;

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
		struct UBO {
			glm::mat4 model{};
			glm::mat4 view{};
			glm::mat4 proj{};
		};
		pipeline = m_gfx->createPipeline(resMan.getFilePath("shader.vert.spv").string().c_str(), resMan.getFilePath("shader.frag.spv").string().c_str(), vertFormat, sizeof(UBO));

		const std::vector<Vertex> vertices = {
			{	{ 0.5f,	-0.5f},	{1.0f, 0.0f, 0.0f}	},
			{	{ 0.5f,	 0.5f},	{0.0f, 1.0f, 0.0f}	},
			{	{-0.5f,	-0.5f},	{0.0f, 0.0f, 1.0f}	},
			{	{-0.5f,	 0.5f},	{0.0f, 1.0f, 1.0f}	},
		};
		vb = m_gfx->createBuffer(gfx::BufferType::VERTEX, sizeof(Vertex) * vertices.size(), vertices.data());
		const std::vector<uint32_t> indices = {
			0, 1, 2, 2, 1, 3,
		};
		ib = m_gfx->createBuffer(gfx::BufferType::INDEX, sizeof(uint32_t) * indices.size(), indices.data());

		UBO initialUbo{};
//		ub = m_gfx->createBuffer(gfx::BufferType::UNIFORM, sizeof(UBO), &initialUbo);

	}

	Application::~Application()
	{
//		m_gfx->destroyBuffer(ub);
		m_gfx->destroyBuffer(ib);
		m_gfx->destroyBuffer(vb);
		m_gfx->destroyPipeline(pipeline);
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

			m_gfx->drawIndexed(pipeline, vb, ib, 6);

			m_gfx->renderFrame();

			/* poll events */
			m_win->getInputAndEvents();

		}

		m_gfx->waitIdle();
	}

}
