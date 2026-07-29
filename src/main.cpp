#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/ui/Popup.hpp>
#include <fstream>
#include <map>
#include <vector>
#include <string>

using namespace geode::prelude;

// ==========================================
// 1. СЛОВАРЬ БУКВ (Сюда будешь вставлять координаты)
// ==========================================
std::map<std::string, std::vector<CCPoint>> cyrillicFont = {
    // Временные координаты для примера
    {"А", {{15, 60}, {0, 30}, {30, 30}, {0, 0}, {30, 0}, {15, 30}}},
    {"Б", {{0, 60}, {15, 60}, {0, 30}, {15, 30}, {0, 0}, {15, 0}, {30, 15}}}
};

// ==========================================
// 2. ЛОГИКА ГЕНЕРАЦИИ ТЕКСТА
// ==========================================
void spawnAutoText(std::string const& text) {
    auto editorLayer = LevelEditorLayer::get();
    if (!editorLayer) return;

    CCPoint startPos = editorLayer->m_objectLayer->getPosition() * -1 + CCPoint { 280, 160 };
    float currentX = startPos.x;
    float currentY = startPos.y;
    float letterSpacing = 60.0f; 

    for (size_t i = 0; i < text.length(); ) {
        std::string letter = "";
        
        if ((text[i] & 0xE0) == 0xC0 && i + 1 < text.length()) {
            letter += text[i];
            letter += text[i+1];
            i += 2;
        } else {
            letter += text[i];
            i++;
        }

        if (letter == " ") {
            currentX += letterSpacing;
            continue;
        }

        if (cyrillicFont.count(letter)) {
            CCArray* spawnedObjects = CCArray::create();

            for (const auto& point : cyrillicFont[letter]) {
                auto obj = GameObject::createWithKey(1); 
                obj->setPosition({currentX + point.x, currentY + point.y});
                
                editorLayer->m_objectLayer->addChild(obj);
                editorLayer->addSpecialibling(obj);
                spawnedObjects->addObject(obj);
            }
            
            editorLayer->m_editorUI->deselectAll();
            editorLayer->m_editorUI->selectObjects(spawnedObjects, false);
        }

        currentX += letterSpacing;
    }
    
    FLAlertLayer::create("Успех!", "Текст выстроен из блоков!", "OK")->show();
}

// ==========================================
// 3. ОКНО ВВОДА ТЕКСТА
// ==========================================
class TextGenPopup : public Popup<std::string const&> {
protected:
    CCTextInputNode* m_inputNode;

    bool setup(std::string const& value) override {
        this->setTitle("Русский Текст GD");
        auto winSize = CCDirector::sharedDirector()->getWinSize();

        m_inputNode = CCTextInputNode::create(220.f, 40.f, "Введите текст...", "bigFont.fnt");
        m_inputNode->setPosition(winSize / 2);
        m_inputNode->setMaxLabelLength(15);
        this->m_mainLayer->addChild(m_inputNode);

        auto confirmBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Создать", "goldBtn_001.png", ccp(0,0), 0.8f),
            this,
            menu_selector(TextGenPopup::onConfirm)
        );
        confirmBtn->setPosition({winSize.width / 2, winSize.height / 2 - 50});
        
        auto menu = CCMenu::create();
        menu->addChild(confirmBtn);
        menu->setPosition({0, 0});
        this->m_mainLayer->addChild(menu);

        return true;
    }

    void onConfirm(CCObject* sender) {
        std::string inputText = m_inputNode->getString();
        spawnAutoText(inputText);
        this->onClose(sender);
    }

public:
    static TextGenPopup* create() {
        auto ret = new TextGenPopup();
        if (ret && ret->init(280.f, 160.f, "GJ_square01.png")) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

// ==========================================
// 4. КНОПКИ И ЭКСТРАКТОР (HOOKS)
// ==========================================
class $modify(MyEditorUI, EditorUI) {
    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;

        auto btnSprite = CCSprite::createWithSpriteFrameName("GJ_chatBtn_001.png");
        auto textBtn = CCMenuItemSpriteExtra::create(
            btnSprite, this, menu_selector(MyEditorUI::onOpenTextGen)
        );

        auto extractSprite = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
        auto extractBtn = CCMenuItemSpriteExtra::create(
            extractSprite, this, menu_selector(MyEditorUI::onExtractCoordinates)
        );

        if (auto menu = this->getChildByID("toolbar-categories-menu")) {
            menu->addChild(textBtn);
            menu->addChild(extractBtn);
            menu->updateLayout();
        }

        return true;
    }

    void onOpenTextGen(CCObject* sender) {
        TextGenPopup::create()->show();
    }

    // Тот самый экстрактор: сохраняет C++ код выделенной буквы в файл
    void onExtractCoordinates(CCObject* sender) {
        auto selectedObjs = this->m_selectedObjects;
        
        if (!selectedObjs || selectedObjs->count() == 0) {
            FLAlertLayer::create("Ошибка", "Выдели объекты буквы на уровне!", "OK")->show();
            return;
        }

        std::string cppCode = "{\n    ";
        auto baseObj = static_cast<GameObject*>(selectedObjs->objectAtIndex(0));
        float baseX = baseObj->getPositionX();
        float baseY = baseObj->getPositionY();

        for (int i = 0; i < selectedObjs->count(); i++) {
            auto obj = static_cast<GameObject*>(selectedObjs->objectAtIndex(i));
            float offsetX = obj->getPositionX() - baseX;
            float offsetY = obj->getPositionY() - baseY;

            cppCode += fmt::format("{{{}, {}}}", offsetX, offsetY);
            if (i < selectedObjs->count() - 1) cppCode += ", ";
        }
        cppCode += "\n}\n\n";

        // Сохраняем в папку сохранения мода на телефоне
        std::filesystem::path savePath = Mod::get()->getSaveDir() / "extracted_letters.txt";

        std::ofstream outFile(savePath, std::ios::app);
        if (outFile.is_open()) {
            outFile << cppCode;
            outFile.close();
            
            std::string msg = "Записано в файл!\n" + savePath.string();
            FLAlertLayer::create("Успех", msg.c_str(), "OK")->show();
        } else {
            FLAlertLayer::create("Ошибка", "Не удалось записать файл!", "OK")->show();
        }
    }
};
