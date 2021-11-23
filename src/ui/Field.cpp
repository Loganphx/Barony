// Field.cpp

#include "../main.hpp"
#include "../draw.hpp"
#include "../player.hpp"
#include "Frame.hpp"
#include "Field.hpp"
#include "Text.hpp"
#include "GameUI.hpp"
#include "Image.hpp"
#include <cassert>

Field::Field(const int _textLen) {
	textlen = std::max(_textLen, 0);
	text = new char[textlen + 1];
	color = makeColor(255, 255, 255, 255);
	textColor = makeColor(255, 255, 255, 255);
	outlineColor = makeColor(0, 0, 0, 255);
	memset(text, 0, textlen + 1);
}

Field::Field(const char* _text) {
	textlen = strlen(_text);
	text = new char[textlen + 1];
	color = makeColor(255, 255, 255, 255);
	textColor = makeColor(255, 255, 255, 255);
	outlineColor = makeColor(0, 0, 0, 255);
	setText(_text);
}

Field::Field(Frame& _parent, const int _textLen) : Field(_textLen) {
	parent = &_parent;
	_parent.getFields().push_back(this);
	_parent.adoptWidget(*this);
}

Field::Field(Frame& _parent, const char* _text) : Field(_text) {
	parent = &_parent;
	_parent.getFields().push_back(this);
	_parent.adoptWidget(*this);
}

Field::~Field() {
	deselect();
	deactivate();
	if (text) {
		if (inputstr == text) {
			inputstr = nullptr;
			inputlen = 0;
			SDL_StopTextInput();
		}
		delete[] text;
		text = nullptr;
	}
}

void Field::activate() {
	Widget::select();
	if (!editable) {
		return;
	}
#ifdef NINTENDO
	auto result = nxKeyboard(guide.c_str());
	if (result.success) {
		setText(result.str.c_str());
	}
#else
	activated = true;
	inputstr = text;
	inputlen = textlen;
	SDL_StartTextInput();
#endif
}

void Field::deselect() {
	Widget::deselect();
}

void Field::deactivate() {
	activated = false;
	selectAll = false;
	if (inputstr == text) {
		inputstr = nullptr;
		inputlen = 0;
		SDL_StopTextInput();
	}
}

char* Field::tokenize(char* str, const char* const delimiters) {
	if (!str || !delimiters) {
		return nullptr;
	}
	size_t del_len = strlen(delimiters);
	for (char* token = str;; ++token) {
		for (size_t c = 0; c < del_len; ++c) {
			if (*token == delimiters[c]) {
				*token = '\0';
				return token + 1;
			}
		}
		if (*token == '\0') {
			return nullptr;
		}
	}
}

void Field::draw(SDL_Rect _size, SDL_Rect _actualSize, const std::vector<const Widget*>& selectedWidgets) const {
	if ( invisible || isDisabled() ) {
		return;
	}

	SDL_Rect rect;
	rect.x = _size.x + std::max(0, size.x - _actualSize.x);
	rect.y = _size.y + std::max(0, size.y - _actualSize.y);
	rect.w = std::min(size.w, _size.w - size.x + _actualSize.x) + std::min(0, size.x - _actualSize.x);
	rect.h = std::min(size.h, _size.h - size.y + _actualSize.y) + std::min(0, size.y - _actualSize.y);
	if (rect.w <= 0 || rect.h <= 0)
		return;

	SDL_Rect scaledRect;
	scaledRect.x = rect.x;
	scaledRect.y = rect.y;
	scaledRect.w = rect.w;
	scaledRect.h = rect.h;

	auto white = Image::get("images/system/white.png");
	const SDL_Rect viewport{ 0, 0, Frame::virtualScreenX, Frame::virtualScreenY };
	if (activated && selectAll) {
		white->drawColor(nullptr, scaledRect, viewport, backgroundSelectAllColor);
	} else if (activated) {
		white->drawColor(nullptr, scaledRect, viewport, backgroundActivatedColor);
	} else {
		white->drawColor(nullptr, scaledRect, viewport, backgroundColor);
	}

	bool showCursor = (ticks - cursorflash) % TICKS_PER_SECOND < TICKS_PER_SECOND / 2;

	Font* actualFont = Font::get(font.c_str());
	if (!actualFont) {
		return;
	}
	int lines = std::max(1, getNumTextLines());
	int fullH = lines * (actualFont->height(false) + actualFont->getOutline() * 2);

	char* buf = (char*)malloc(textlen + 1);
	memcpy(buf, text ? text : "\0", textlen + 1);

	int yoff = 0;
	char* nexttoken;
	char* token = buf;
	do {
		nexttoken = tokenize(token, "\n");

		Text* text = Text::get(token, font.c_str(), textColor, outlineColor);
		assert(text);

		// get the size of the rendered text
		int textSizeW = text->getWidth();
		int textSizeH = text->getHeight();

		SDL_Rect pos;
		if (hjustify == LEFT || hjustify == TOP) {
			pos.x = _size.x + size.x - _actualSize.x;
		} else if (hjustify == CENTER) {
			pos.x = _size.x + size.x + size.w / 2 - textSizeW / 2 - _actualSize.x;
		} else if (hjustify == RIGHT || hjustify == BOTTOM) {
			pos.x = _size.x + size.x + size.w - textSizeW - _actualSize.x;
		}
		if (vjustify == LEFT || vjustify == TOP) {
			pos.y = _size.y + size.y + yoff - _actualSize.y + std::min(size.h - fullH, 0);
		} else if (vjustify == CENTER) {
			pos.y = _size.y + size.y + yoff - _actualSize.y + (size.h - fullH) / 2;
		} else if (vjustify == RIGHT || vjustify == BOTTOM) {
			pos.y = _size.y + size.y + yoff - _actualSize.y + std::max(size.h - fullH, 0);
		}
		pos.w = textSizeW;
		pos.h = textSizeH;

		yoff += actualFont->height(true);

		SDL_Rect dest;
		dest.x = std::max(rect.x, pos.x);
		dest.y = std::max(rect.y, pos.y);
		dest.w = pos.w - (dest.x - pos.x) - std::max(0, (pos.x + pos.w) - (rect.x + rect.w));
		dest.h = pos.h - (dest.y - pos.y) - std::max(0, (pos.y + pos.h) - (rect.y + rect.h));

		SDL_Rect src;
		src.x = std::max(0, rect.x - pos.x);
		src.y = std::max(0, rect.y - pos.y);
		src.w = pos.w - (dest.x - pos.x) - std::max(0, (pos.x + pos.w) - (rect.x + rect.w));
		src.h = pos.h - (dest.y - pos.y) - std::max(0, (pos.y + pos.h) - (rect.y + rect.h));

		// fit text to window
		if ((hjustify == LEFT || hjustify == TOP) && scroll && activated) {
			src.x = std::max(src.x, textSizeW - rect.w);
		}

		if (src.w <= 0 || src.h <= 0 || dest.w <= 0 || dest.h <= 0) {
			continue;
		}

		SDL_Rect scaledDest;
		scaledDest.x = dest.x;
		scaledDest.y = dest.y;
		scaledDest.w = dest.w;
		scaledDest.h = dest.h;

		if (parent && static_cast<Frame*>(parent)->getOpacity() < 100.0) {
			Uint8 r, g, b, a;
			SDL_GetRGBA(color, mainsurface->format, &r, &g, &b, &a);
			a *= static_cast<Frame*>(parent)->getOpacity() / 100.0;
			text->drawColor(src, scaledDest, viewport, SDL_MapRGBA(mainsurface->format, r, g, b, a));
		} else {
			text->drawColor(src, scaledDest, viewport, color);
		}

		// draw cursor
		if (!nexttoken && showCursor && activated) {
			SDL_Rect cursorSize{scaledDest.x + scaledDest.w - 2, scaledDest.y, 2, scaledDest.h};
			white->drawColor(nullptr, cursorSize, viewport, color);
		}
	} while ((token = nexttoken) != NULL);

	free(buf);

	// draw user stuff
	if (drawCallback) {
		drawCallback(*this, scaledRect);
	}
}

void Field::drawPost(SDL_Rect _size, SDL_Rect _actualSize, const std::vector<const Widget*>& selectedWidgets) const {
	if (invisible) {
		return;
	}
	SDL_Rect rect;
	rect.x = _size.x + std::max(0, size.x - _actualSize.x);
	rect.y = _size.y + std::max(0, size.y - _actualSize.y);
	rect.w = std::min(size.w, _size.w - size.x + _actualSize.x) + std::min(0, size.x - _actualSize.x);
	rect.h = std::min(size.h, _size.h - size.y + _actualSize.y) + std::min(0, size.y - _actualSize.y);
	if (rect.w <= 0 || rect.h <= 0) {
		return;
	}
	Widget::drawPost(rect, selectedWidgets);
}

Field::result_t Field::process(SDL_Rect _size, SDL_Rect _actualSize, const bool usable) {
	Widget::process();

	result_t result;
	result.highlighted = false;
	result.entered = false;
	if (!editable) {
		if (activated) {
			result.entered = true;
			deactivate();
		}
		return result;
	}

	_size.x += std::max(0, size.x - _actualSize.x);
	_size.y += std::max(0, size.y - _actualSize.y);
	_size.w = std::min(size.w, _size.w - size.x + _actualSize.x) + std::min(0, size.x - _actualSize.x);
	_size.h = std::min(size.h, _size.h - size.y + _actualSize.y) + std::min(0, size.y - _actualSize.y);
	if (_size.w <= 0 || _size.h <= 0) {
		return result;
	}

	Sint32 omousex = (::omousex / (float)xres) * (float)Frame::virtualScreenX;
	Sint32 omousey = (::omousey / (float)yres) * (float)Frame::virtualScreenY;

	if (activated) {
		if (inputstr != text) {
			result.entered = true;
			deactivate();
			if (inputstr == nullptr) {
				SDL_StopTextInput();
			}
		}
		if (Input::keys[SDL_SCANCODE_RETURN] || Input::keys[SDL_SCANCODE_KP_ENTER]) {
			Input::keys[SDL_SCANCODE_RETURN] = 0;
			Input::keys[SDL_SCANCODE_KP_ENTER] = 0;
			result.entered = true;
			deactivate();
		}
		if (Input::keys[SDL_SCANCODE_ESCAPE]) {
			Input::keys[SDL_SCANCODE_ESCAPE] = 0;
			result.entered = true;
			deactivate();
		}

		/*if (selectAll) {
			if (mainEngine->getAnyKeyStatus()) {
				const char* keys = lastkeypressed;
				if (keys) {
					text = keys;
					selectAll = false;
				}
			}
		}*/
	}

#if !defined(NINTENDO) && !defined(EDITOR)
	if (inputs.getVirtualMouse(owner)->draw_cursor) {
		if (omousex >= _size.x && omousex < _size.x + _size.w &&
			omousey >= _size.y && omousey < _size.y + _size.h) {
			result.highlighted = true;
		}
	}

	if (!result.highlighted && mousestatus[SDL_BUTTON_LEFT]) {
		if (activated) {
			result.entered = true;
			deactivate();
		}
	} else if (result.highlighted && mousestatus[SDL_BUTTON_LEFT]) {
		activate();
		/*if (doubleclick_mousestatus[SDL_BUTTON_LEFT]) {
			selectAll = true;
		}*/
	}
#endif

	return result;
}

void Field::setText(const char* _text) {
	if (_text == nullptr) {
		return;
	}
	int size = std::min(std::max(0, (int)strlen(_text)), (int)textlen);
	if (size > 0) {
		memcpy(text, _text, size);
	}
	text[size] = '\0';
}

void Field::scrollParent() {
	Frame* fparent = static_cast<Frame*>(parent);
	auto fActualSize = fparent->getActualSize();
	auto fSize = fparent->getSize();
	if (size.y < fActualSize.y) {
		fActualSize.y = size.y;
	}
	else if (size.y + size.h >= fActualSize.y + fSize.h) {
		fActualSize.y = (size.y + size.h) - fSize.h;
	}
	if (size.x < fActualSize.x) {
		fActualSize.x = size.x;
	}
	else if (size.x + size.w >= fActualSize.x + fSize.w) {
		fActualSize.x = (size.x + size.w) - fSize.w;
	}
	fparent->setActualSize(fActualSize);
}

std::unordered_map<size_t, std::string> reflowTextLine(std::string& input, int width, const char* font)
{
	std::unordered_map<size_t, std::string> result;

	Font* actualFont = Font::get(font);
	if ( !actualFont )
	{
		return result;
	}
	int charWidth = 0;
	actualFont->sizeText("_", &charWidth, nullptr);
	if ( charWidth == 0 )
	{
		return result;
	}
	++charWidth;

	const int charactersPerLine = (width) / charWidth;

	size_t offset = 0;
	size_t findChar = 0;
	std::vector<std::string> tokens;
	/*switch ( rand() % 3 )
	{
		case 0:
		input = " this is a test ";
			break;
		case 1:
		input = " this is  a test  ";
			break;
		case 2:
		input = "  this is  a test !";
			break;
	}*/
	while ( (findChar = input.find(' ', offset)) != std::string::npos ) {
		tokens.push_back(input.substr(offset, findChar - offset));
		offset = findChar + 1;
	}
	tokens.push_back(input.substr(offset));

	size_t currentLine = 0;
	Text* getText = nullptr;
	bool lastInsertedManualSpace = false;
	for ( auto& token : tokens )
	{
		size_t currentLength = result[currentLine].size();
		if ( (currentLength + 1 + token.size() < charactersPerLine / 2) )
		{
			// this is probably OK
		}
		else if ( getText = Text::get(std::string(result[currentLine] + " " + token).c_str(), font,
			makeColor(255, 255, 255, 255), makeColor(0, 0, 0, 255)) )
		{
			if ( getText->getWidth() > width )
			{
				++currentLine;
			}
		}

		if ( result[currentLine] == "" )
		{
			if ( token == "" && tokens.size() > 1 ) // if there's only 1 token "" then it doesn't need a space.
			{
				lastInsertedManualSpace = true;
				result[currentLine] = " ";
			}
			else
			{
				result[currentLine] = token;
			}
		}
		else
		{
			if ( !lastInsertedManualSpace )
			{
				result[currentLine].append(" ");
			}
			lastInsertedManualSpace = false;
			result[currentLine].append(token);
		}
	}
	/*if ( result.size() == 1 )
	{
		if ( result[0] != input )
		{
			printlog("Inequality: |%s|%s|", result[0].c_str(), input.c_str());
		}
		assert(result[0] == input);
	}*/
	return result;
}

std::string Field::getLongestLine()
{
	if ( text == nullptr || textlen <= 1 ) {
		return "";
	}
	if ( getNumTextLines() <= 1 )
	{
		return text;
	}
	char* nexttoken;
	char* token = text;
	std::string originalText = text;
	std::string longestLine = "";
	int longestLineWidth = 0;
	do {
		nexttoken = tokenize(token, "\n");
		if ( auto getText = Text::get(token, font.c_str(), textColor, outlineColor) )
		{
			if ( getText->getWidth() > longestLineWidth )
			{
				longestLineWidth = getText->getWidth();
				longestLine = token;
			}
		}
	} while ( (token = nexttoken) != NULL );
	setText(originalText.c_str()); // make sure to replace the original text field, as tokenize will modify it
	return longestLine;
}

int Field::getLastLineThatFitsWithinHeight()
{
	if ( text == nullptr || textlen <= 1 ) {
		return -1;
	}
	if ( auto getText = Text::get(text, font.c_str(), textColor, outlineColor) )
	{
		if ( getText->getHeight() <= getSize().h/* - getSize().y*/ )
		{
			return -1;
		}
	}
	else
	{
		return -1;
	}

	std::string allLines;
	int lineNumber = 0;
	char* nexttoken;
	char* token = text;
	std::string originalText = text;
	do {
		nexttoken = tokenize(token, "\n");
		if ( !allLines.empty() )
		{
			allLines.push_back('\n');
		}
		allLines += token[0];
		if ( auto getText = Text::get(allLines.c_str(), font.c_str(), textColor, outlineColor) )
		{
			if ( getText->getHeight() > getSize().h )
			{
				// doesn't fit, return the last line number.
				setText(originalText.c_str()); // make sure to replace the original text field, as tokenize will modify it
				return lineNumber;
			}
		}
		++lineNumber;
	} while ( (token = nexttoken) != NULL );
	setText(originalText.c_str()); // make sure to replace the original text field, as tokenize will modify it
	return -1;
}

void Field::reflowTextToFit(const int characterOffset) {
	if ( text == nullptr || textlen <= 1 ) {
		return;
	}

	if ( auto getText = Text::get(text, font.c_str(), textColor, outlineColor) )
	{
		if ( getText->getWidth() <= (getSize().w) )
		{
			// no work to do
			return;
		}
	}
	std::string reflowText = "";

#ifndef EDITOR
	bool usePreciseStringWidth = bUsePreciseFieldTextReflow;
#else
	bool usePreciseStringWidth = true;
#endif
	if ( usePreciseStringWidth )
	{
		// more expensive, but accurate text reflow.
		std::vector<std::string> allLines;
		char* nexttoken;
		char* token = text;
		do {
			nexttoken = tokenize(token, "\n");
			auto result = reflowTextLine(std::string(token), (getSize().w), font.c_str());
			for ( size_t i = 0; i < result.size(); ++i )
			{
				allLines.push_back(result[i]);
			}
		} while ( (token = nexttoken) != NULL );

		for ( auto it = allLines.begin(); it != allLines.end(); ++it )
		{
			reflowText += (*it);
			auto nextStr = std::next(it);
			if ( nextStr != allLines.end() )
			{
				reflowText += '\n';
			}
		}
		setText(reflowText.c_str());
		return;
	}

	Font* actualFont = Font::get(font.c_str());
	if ( !actualFont )
	{
		return;
	}
	
	int charWidth = 0;
	actualFont->sizeText("_", &charWidth, nullptr);
	//if ( auto textGet = Text::get("_", font.c_str(), textColor, outlineColor) )
	//{
	//	charWidth = textGet->getWidth();
	//}

	if ( charWidth == 0 )
	{
		return;
	}
	++charWidth;
	const int charactersPerLine = (getSize().w) / charWidth;

	int currentCharacters = 0;
	for ( int i = 0; text[i] != '\0'; ++i )
	{
		if ( (currentCharacters - characterOffset) > charactersPerLine )
		{
			int findSpace = reflowText.rfind(' ', reflowText.size());
			if ( findSpace != std::string::npos )
			{
				int lastWordEnd = reflowText.size();
				reflowText.at(findSpace) = '\n';
				currentCharacters = lastWordEnd - findSpace;
			}
			else
			{
				reflowText += '\n';
				currentCharacters = 0;
			}

			reflowText += text[i];
		}
		else
		{
			if ( text[i] == '\n' )
			{
				currentCharacters = 0;
			}
			++currentCharacters;
			reflowText += text[i];
		}
	}

	setText(reflowText.c_str());
}

int Field::getNumTextLines() const {
	return Text::get(text, font.c_str(), textColor, outlineColor)->getNumTextLines();
}

SDL_Rect Field::getAbsoluteSize() const
{
	SDL_Rect _size{ size.x, size.y, size.w, size.h };
	auto _parent = static_cast<Frame*>(this->parent);
	if ( _parent ) {
		SDL_Rect absoluteSize = _parent->getAbsoluteSize();
		_size.x += std::max(0, absoluteSize.x);
		_size.y += std::max(0, absoluteSize.y);
	}
	return _size;
}