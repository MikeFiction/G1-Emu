#include "Overlay.h"

namespace g1gui
{
	namespace
	{
		constexpr float g_faceDim = 0.5f, g_bodyAlpha = 0.7f;	// the synth darkened around the card, which lets it through
		constexpr float g_backdropScale = 0.25f;	// the panel is photographed small and blurred: cheap, and soft
		constexpr int g_confirmW = 480, g_confirmH = 170;
	}

	juce::Image overlay::backdrop(juce::Component& _behind)
	{
		if(_behind.getLocalBounds().isEmpty())
			return {};
		auto image = _behind.createComponentSnapshot(_behind.getLocalBounds(), true, g_backdropScale);
		juce::ImageConvolutionKernel blur(7);
		blur.createGaussianBlur(2.5f);
		const auto sharp = image.createCopy();
		blur.applyToImage(image, sharp, image.getBounds());
		return image;
	}

	// Only what is behind the card is blurred, and shows through its body; the synth around it is
	// darkened, and the rest of the panel stays as it is.
	void overlay::paintCard(juce::Graphics& _g, const juce::Rectangle<int> _area, const juce::Image& _backdrop, const int _faceHeight,
		const juce::Rectangle<int> _card, const juce::String& _title)
	{
		juce::Path shape;
		shape.addRoundedRectangle(_card.toFloat(), 10.0f);
		if(_backdrop.isValid())
		{
			juce::Graphics::ScopedSaveState state(_g);
			_g.reduceClipRegion(shape);
			_g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
			_g.drawImage(_backdrop, _area.toFloat());
		}
		_g.setColour(juce::Colours::black.withAlpha(g_faceDim));
		_g.fillRect(_area.withHeight(_faceHeight));
		_g.setColour(Body.withAlpha(g_bodyAlpha));
		_g.fillPath(shape);
		_g.setColour(Rule);
		_g.drawRoundedRectangle(_card.toFloat().reduced(0.5f), 10.0f, 1.0f);
		_g.drawLine(static_cast<float>(_card.getX() + Margin), static_cast<float>(_card.getY() + TitleRuleY),
			static_cast<float>(_card.getRight() - Margin), static_cast<float>(_card.getY() + TitleRuleY));

		_g.setColour(juce::Colours::white);
		_g.setFont(juce::FontOptions(HeadingSize, juce::Font::bold));
		_g.drawText(_title, _card.getX() + Margin, _card.getY() + 12, _card.getWidth() - 2 * Margin, 22, juce::Justification::centredLeft);
	}

	ConfirmView::ConfirmView()
	{
		setWantsKeyboardFocus(true);
		m_text.setFont(juce::FontOptions(overlay::TextSize));
		m_text.setColour(juce::Label::textColourId, overlay::NoteText);
		m_text.setJustificationType(juce::Justification::topLeft);
		addAndMakeVisible(m_text);
		m_yes.onClick = [this] { answer(true); };
		m_cancel.onClick = [this] { answer(false); };
		addAndMakeVisible(m_yes);
		addAndMakeVisible(m_cancel);
	}

	void ConfirmView::open(juce::Component& _behind, const int _faceHeight, const juce::Rectangle<int> _space, const juce::String& _title,
		const juce::String& _text, const juce::String& _yes, std::function<void()> _onYes)
	{
		m_backdrop = overlay::backdrop(_behind);
		m_faceHeight = _faceHeight;
		m_space = _space;
		m_title = _title.toUpperCase();
		m_text.setText(_text, juce::dontSendNotification);
		m_yes.setButtonText(_yes);
		m_onYes = std::move(_onYes);
		setBounds(_behind.getLocalBounds());
		resized();
		setVisible(true);
		toFront(true);
		grabKeyboardFocus();
	}

	void ConfirmView::close()
	{
		setVisible(false);
		m_backdrop = {};
		if(onClose)
			onClose();
	}

	void ConfirmView::answer(const bool _yes)
	{
		auto yes = std::move(m_onYes);
		m_onYes = nullptr;
		close();
		if(_yes && yes)
			yes();
	}

	juce::Rectangle<int> ConfirmView::card() const
	{
		const auto space = m_space.isEmpty() ? getLocalBounds() : m_space;
		return juce::Rectangle<int>(g_confirmW, g_confirmH).withCentre(space.getCentre());
	}

	void ConfirmView::paint(juce::Graphics& _g)
	{
		overlay::paintCard(_g, getLocalBounds(), m_backdrop, m_faceHeight, card(), m_title);
	}

	// The text under the title, the buttons at the bottom right: Cancel last, as Close is on the
	// Synth Settings.
	void ConfirmView::resized()
	{
		const auto c = card();
		const int m = overlay::Margin;
		m_text.setBounds(c.getX() + m - 4, c.getY() + overlay::TitleRuleY + 10, c.getWidth() - 2 * m + 8, 60);
		m_cancel.setBounds(c.getRight() - m - 90, c.getBottom() - 18 - 26, 90, 26);
		m_yes.setBounds(m_cancel.getX() - 10 - 90, m_cancel.getY(), 90, 26);
	}

	void ConfirmView::mouseDown(const juce::MouseEvent& _e)
	{
		if(!card().contains(_e.getPosition()))
			answer(false);
	}

	bool ConfirmView::keyPressed(const juce::KeyPress& _key)
	{
		if(_key == juce::KeyPress::escapeKey)
			answer(false);
		else if(_key == juce::KeyPress::returnKey)
			answer(true);
		else
			return false;
		return true;
	}
}
