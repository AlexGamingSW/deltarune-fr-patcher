#include "UI/UpdaterWindow.hpp"
#include "BPSParser.hpp"
#include "utils.hpp"
#include <boxer/boxer.h>
#include <nfd.h>

namespace drfr
{
	sf::View UpdaterWindow::getLetterboxView()
	{
		sf::View view = window.getView();
		float windowRatio = window.getSize().x / static_cast<float>(window.getSize().y);
		float viewRatio = view.getSize().x / static_cast<float>(view.getSize().y);
		float sizeX = 1, sizeY = 1, posX = 0, posY = 0;

		bool horizontalSpacing = true;
		if (windowRatio < viewRatio)
			horizontalSpacing = false;

		if (horizontalSpacing)
		{
			sizeX = viewRatio / windowRatio;
			posX = (1 - sizeX) / 2.f;
		}
		else
		{
			sizeY = windowRatio / viewRatio;
			posY = (1 - sizeY) / 2.f;
		}

		view.setViewport(sf::FloatRect(sf::Vector2f(posX, posY), sf::Vector2f(sizeX, sizeY)));
		return view;
	}

	void UpdaterWindow::init()
	{
		window.create(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "DELTARUNE [Patch FR]");
		previousResolution = window.getView().getSize();

		if (!icon.loadFromFile("assets/icon.png"))
			throw std::runtime_error("Failed to load icon");
		window.setIcon(icon.getSize().x, icon.getSize().y, icon.getPixelsPtr());

		if (!this->font.loadFromFile("assets/determination.ttf"))
			throw std::runtime_error("Failed to load font");
		utils::getToString("https://deltaruneapi.mbourand.fr/updater/version.txt", this->latestVersion);

		if (!backgroundImg.loadFromFile("assets/bg.png"))
			throw std::runtime_error("Failed to load background image");
		background = sf::Sprite(backgroundImg);

		if (!logoImg.loadFromFile("assets/logo.png"))
			throw std::runtime_error("Failed to load logo image");
		logo = sf::Sprite(logoImg);

		sf::Vector2f installPos(window.getSize().x / 2 - window.getSize().x * 0.25, window.getSize().y * 0.45);
		sf::Vector2f installSize(window.getSize().x * 0.5, window.getSize().y * 0.13);
		installButton = Button(installPos, installSize, L"Installer", 50);

		sf::Vector2f uninstallPos(window.getSize().x / 2 - window.getSize().x * 0.115, window.getSize().y * 0.795);
		sf::Vector2f uninstallSize(window.getSize().x * 0.23, window.getSize().y * 0.07);
		uninstallButton = Button(uninstallPos, uninstallSize, L"Désinstaller", 30);

		sf::Vector2f creditsPos(window.getSize().x / 2 - window.getSize().x * 0.15, window.getSize().y * 0.91);
		sf::Vector2f creditsSize(window.getSize().x * 0.3, window.getSize().y * 0.07);
		creditsButton = Button(creditsPos, creditsSize, L"Crédits", 38);

		sf::Vector2f progressPos(window.getSize().x / 2 - window.getSize().x * 0.225, window.getSize().y * 0.595);
		sf::Vector2f progressSize(window.getSize().x * 0.45, window.getSize().y * 0.02);
		progressBar = ProgressBar(L"", 0, 100);
		progressBar.setPosition(progressPos);
		progressBar.setSize(progressSize);
		progressBar.setEnabled(false);
	}

	void UpdaterWindow::handleEvents()
	{
		while (window.pollEvent(event))
		{
			switch (event.type)
			{
				case sf::Event::Closed:
					window.close();
					break;

				case sf::Event::Resized:
					window.setSize(sf::Vector2u(std::max(event.size.width, 640u), std::max(event.size.height, 480u)));
					sf::View view = this->getLetterboxView();
					window.setView(view);
					this->_scaleElements();
					break;
			}
		}
	}

	void UpdaterWindow::doTick()
	{
		bool mousePreessed = sf::Mouse::isButtonPressed(sf::Mouse::Left);
		installButton.update(window, mousePreessed);
		uninstallButton.update(window, mousePreessed);
		creditsButton.update(window, mousePreessed);

		this->_scaleElements();

		if (creditsButton.isPressed())
			utils::openWebPage("https://deltarune.fr/credits");

		try
		{
			if (installButton.isPressed())
				this->_download();
			if (this->state == State::Downloading)
				this->_updateDownloadProgress();
			if (this->state == State::DoneDownloading)
				this->_applyPatch();
			if (this->state == State::Installing)
				this->_updateInstallProgressBar();
			if (this->state == State::DoneInstalling)
				this->_moveFiles();
		}
		catch (std::exception& e)
		{
			this->state = State::Idle;
			boxer::show(
				(std::string("Une erreur est survenue: ") + e.what() +
				 "\n\nVerifiez que le jeu est a jour, reparez les fichiers, puis reessayez.\nSi vous avez besoin "
				 "d'aide, rendez-vous sur notre discord.")
					.c_str(),
				(std::string("Une erreur est survenue: ") + e.what()).c_str(), boxer::Style::Error);
			this->progressBar.setEnabled(false);
			this->installButton.setEnabled(true);
			this->uninstallButton.setEnabled(true);
		}

		try
		{
			if (uninstallButton.isPressed())
				this->_downloadUninstall();
			if (this->state == State::DownloadUninstall)
				this->_updateDownloadUninstallProgress();
			if (this->state == State::DoneDownloadUninstall)
				this->_applyUninstaller();
			if (this->state == State::ApplyUninstall)
				this->_updateUninstallProgress();
			if (this->state == State::DoneUninstall)
				this->_uninstallFiles();
		}
		catch (std::exception& e)
		{
			this->state = State::Idle;
			boxer::show((std::string("Une erreur est survenue: ") + e.what() +
						 "\n\nLe jeu n'a pas pu etre desinstalle car les fichiers ne correspondent pas, pour "
						 "desinstaller le patch, reparez les fichiers sur steam.\nSi vous avez besoin "
						 "d'aide, rendez-vous sur notre discord.")
							.c_str(),
						(std::string("Une erreur est survenue: ") + e.what()).c_str(), boxer::Style::Error);
			this->progressBar.setEnabled(false);
			this->installButton.setEnabled(true);
			this->uninstallButton.setEnabled(true);
		}
	}

	void UpdaterWindow::render()
	{
		window.clear();
		window.draw(background);
		window.draw(logo);
		installButton.draw(window);
		progressBar.draw(window);
		uninstallButton.draw(window);
		creditsButton.draw(window);

		std::string currentVersion = getVersion();

		std::string fullText = std::string("Dernière Version : v") + latestVersion +
							   "\nVersion Installée : " + (currentVersion.size() ? "v" + currentVersion : "Aucune");
		sf::Text versionText(sf::String::fromUtf8(fullText.begin(), fullText.end()), font, 17);

		versionText.setPosition(sf::Vector2f(10, window.getView().getSize().y - 45));
		versionText.setFillColor(sf::Color::White);

		window.draw(versionText);
		window.display();
	}

	bool UpdaterWindow::isOpen() const { return window.isOpen(); }

	bool UpdaterWindow::isInstalled() const
	{
		std::ifstream in("version.txt");
		if (!in.is_open())
			return false;
		return true;
	}

	std::string UpdaterWindow::getVersion() const
	{
		std::ifstream in("version.txt");
		if (!in.is_open())
			return "";
		std::string version;
		in >> version;
		return version;
	}

	bool UpdaterWindow::isUpToDate() const
	{
		static std::string latest = "";

		if (latest.size() == 0)
		{
			std::string versionRaw;
			utils::getToString("https://deltaruneapi.mbourand.fr/updater/version.txt", versionRaw);
			latest = versionRaw;
		}

		std::ifstream in("version.txt");
		if (!in.is_open())
			return false;
		std::string version;
		in >> version;

		return version == latest;
	}

	void UpdaterWindow::_scaleElements()
	{
		auto viewSize = window.getView().getSize();

		float bgScale = viewSize.y / static_cast<float>(backgroundImg.getSize().y);
		background.setScale(sf::Vector2f(bgScale, bgScale));
		background.setPosition(sf::Vector2f(viewSize.x / 2, viewSize.y / 2));
		background.setOrigin(
			sf::Vector2f(background.getLocalBounds().width / 2, background.getLocalBounds().height / 2));

		float logoScale = (viewSize.x * (2 / 3.0f)) / static_cast<float>(logoImg.getSize().x);
		logo.setScale(sf::Vector2f(logoScale, logoScale));
		logo.setPosition(sf::Vector2f(viewSize.x / 2, viewSize.y * 0.15));
		logo.setOrigin(sf::Vector2f(logo.getLocalBounds().width / 2, logo.getLocalBounds().height / 2));

		sf::Vector2f installPos(viewSize.x / 2 - viewSize.x * 0.25, viewSize.y * 0.45);
		sf::Vector2f installSize(viewSize.x * 0.5, viewSize.y * 0.13);
		sf::Vector2f uninstallPos(viewSize.x / 2 - viewSize.x * 0.115, viewSize.y * 0.795);
		sf::Vector2f uninstallSize(viewSize.x * 0.23, viewSize.y * 0.07);
		sf::Vector2f creditsPos(viewSize.x / 2 - viewSize.x * 0.15, viewSize.y * 0.91);
		sf::Vector2f creditsSize(viewSize.x * 0.3, viewSize.y * 0.07);

		float prevResX = static_cast<float>(previousResolution.x);
		installButton.setRect(installPos, installSize);
		installButton.setFontSize(installButton.getFontSize() * (viewSize.x / prevResX));
		uninstallButton.setRect(uninstallPos, uninstallSize);
		uninstallButton.setFontSize(uninstallButton.getFontSize() * (viewSize.x / prevResX));
		creditsButton.setRect(creditsPos, creditsSize);
		creditsButton.setFontSize(creditsButton.getFontSize() * (viewSize.x / prevResX));

		progressBar.setPosition(sf::Vector2f(viewSize.x / 2 - viewSize.x * 0.225, viewSize.y * 0.595));
		progressBar.setSize(sf::Vector2f(viewSize.x * 0.45, viewSize.y * 0.02));
	}
}