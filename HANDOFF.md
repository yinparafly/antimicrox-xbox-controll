# AntiMicroX Custom Button Label Feature - Handoff Document

## 项目概述

本次修改为 AntiMicroX 游戏手柄映射工具添加了**自定义按钮功能标签**和**显示模式切换**功能，使用户能够为每个按钮设置易于理解的功能名称，并灵活切换映射显示和功能名称的显示模式。

---

## 修改内容总览

### 1. 核心功能添加

#### A. JoyButton 类增强 (`src/joybuttontypes/joybutton.h/.cpp`)

**新增成员变量：**
```cpp
QString functionLabel;              // 存储自定义功能标签
DisplayMode displayMode;            // 显示模式（映射模式 vs 功能模式）
```

**新增枚举：**
```cpp
enum DisplayMode {
    MappingMode = 0,  // 显示原始键盘/鼠标映射
    FunctionMode = 1  // 显示自定义功能标签
};
```

**新增公共接口：**
- `setFunctionLabel(QString label)` - 设置自定义标签，最多 100 字符
- `getFunctionLabel() const` - 获取当前标签
- `setDisplayMode(DisplayMode mode)` - 切换显示模式
- `getDisplayMode() const` - 获取当前显示模式

**新增信号：**
- `functionLabelChanged()` - 当标签改变时发出
- `displayModeChanged()` - 当显示模式改变时发出

**架构特点：**
- 使用枚举确保类型安全
- 标签长度限制防止 UI 溢出
- 信号机制支持 Qt 信号槽连接
- 默认模式为 `MappingMode`，保持向后兼容

---

#### B. UI 层更新 (`src/gui/joybuttonwidget.cpp`)

**标签生成逻辑 (`generateLabel()`)：**
```cpp
QString JoyButtonWidget::generateLabel()
{
    if (m_button == nullptr)
        return QString();
    
    QString temp;
    // 优先级：功能模式且有标签 > 原始映射名
    if (m_button->getDisplayMode() == JoyButton::FunctionMode 
        && !m_button->getFunctionLabel().isEmpty())
    {
        temp = m_button->getFunctionLabel();
    } else {
        temp = m_button->getName(false, ifDisplayNames());
    }
    
    temp.replace("&", "&&");  // Qt 按钮文本转义
    return temp;
}
```

**信号连接优化：**
- 增加了两个新信号的连接：`functionLabelChanged` 和 `displayModeChanged`
- 两者都会触发 `refreshLabel()`，实时更新 UI
- 添加了 null 指针检查，增强鲁棒性

**连接时机：**
```cpp
// 构造函数中的信号连接
connect(m_button, &JoyButton::functionLabelChanged, this, &JoyButtonWidget::refreshLabel);
connect(m_button, &JoyButton::displayModeChanged, this, &JoyButtonWidget::refreshLabel);
```

**架构特点：**
- 响应式设计：任何属性改变立即反映在 UI
- 条件优先级清晰：FunctionMode + Label > Mapping Name
- 故障转移安全：无标签或非功能模式自动回退到映射名

---

#### C. XML 持久化 (`src/xml/joybuttonxml.cpp`)

**配置读取 (`readConfig()`)：**
```cpp
// 在 XML 中查找和读取自定义标签和显示模式
QString functionLabel = buttonElement.attribute("functionLabel", "");
QString displayModeStr = buttonElement.attribute("displayMode", "0");

if (!functionLabel.isEmpty()) {
    m_joyButton->setFunctionLabel(functionLabel);
}

int mode = displayModeStr.toInt();
m_joyButton->setDisplayMode(static_cast<JoyButton::DisplayMode>(mode));
```

**配置写入 (`writeConfig()`)：**
```cpp
// 将标签和模式保存到 XML
buttonElement.setAttribute("functionLabel", m_joyButton->getFunctionLabel());
buttonElement.setAttribute("displayMode", 
    QString::number(static_cast<int>(m_joyButton->getDisplayMode())));
```

**XML 格式示例：**
```xml
<button index="0" 
        functionLabel="Jump Action" 
        displayMode="1">
    <!-- 其他按钮配置 -->
</button>
```

**架构特点：**
- 属性使用 XML attribute，便于解析
- 枚举值转为整数存储（0=MappingMode, 1=FunctionMode）
- 默认值机制确保向后兼容旧版本配置

---

## 架构设计

### 分层架构

```
┌─────────────────────────────────────────────┐
│         UI 层 (JoyButtonWidget)             │
│  • 显示按钮标签                             │
│  • 监听标签和模式变更信号                   │
│  • 实时刷新显示                             │
└────────────┬────────────────────────────────┘
             │ 信号/槽连接
             ▼
┌─────────────────────────────────────────────┐
│       业务逻辑层 (JoyButton)                │
│  • 存储功能标签和显示模式                   │
│  • 管理标签有效性（长度限制）               │
│  • 发出属性变更信号                         │
│  • 提供标签和模式的 getter/setter            │
└────────────┬────────────────────────────────┘
             │ 数据变更时
             ▼
┌─────────────────────────────────────────────┐
│       持久化层 (JoyButtonXml)               │
│  • 读取配置文件中的标签和模式               │
│  • 写入标签和模式到配置文件                 │
│  • XML attribute 管理                       │
└─────────────────────────────────────────────┘
```

### 信号流

```
用户修改标签
  ↓
JoyButton::setFunctionLabel()
  ↓
emit functionLabelChanged()
  ↓
JoyButtonWidget::refreshLabel()
  ↓
JoyButtonWidget::generateLabel()
  ↓
UI 按钮文本更新
```

### 数据流

```
配置文件 (XML)
  ↓ 应用启动
JoyButtonXml::readConfig()
  ↓
JoyButton::setFunctionLabel/setDisplayMode
  ↓
内存中运行
  ↓ 应用退出/保存
JoyButtonXml::writeConfig()
  ↓
配置文件 (XML)
```

---

## 关键设计决策

### 1. 枚举 vs 布尔值
**决策：** 使用 `enum DisplayMode` 而非 `bool`
- **优势：** 扩展性强，可轻松添加更多模式（如 CustomMode、HybridMode）
- **可读性：** `FunctionMode` 比 `displayNames=true` 更清晰

### 2. 标签长度限制（100 字符）
- **原因：** 防止按钮 UI 溢出
- **权衡：** 足够容纳详细功能描述，又不至过长

### 3. 优先级设计
功能显示优先级：
```
IF (displayMode == FunctionMode) AND (functionLabel.isEmpty() == false)
  THEN 显示 functionLabel
ELSE 显示原始映射名
```
- **好处：** 用户可灵活选择显示方式
- **降级方案：** 无标签时自动回退，不会出现空白按钮

### 4. 信号驱动更新
- **优势：** 解耦 UI 和业务逻辑
- **响应性：** 任何属性变化都能实时在 UI 上反映
- **可维护性：** 修改显示逻辑无需改动 JoyButton 类

---

## 修改文件清单

| 文件 | 修改类型 | 主要改动 |
|-----|--------|--------|
| `src/joybuttontypes/joybutton.h` | 头文件修改 | 新增枚举、成员变量、方法声明、信号 |
| `src/joybuttontypes/joybutton.cpp` | 实现文件修改 | 新增 setter/getter 实现、初始化逻辑 |
| `src/gui/joybuttonwidget.cpp` | UI 文件修改 | 增强 generateLabel()、补充信号连接、加强 null 检查 |
| `src/xml/joybuttonxml.cpp` | XML 处理修改 | readConfig/writeConfig 中添加标签和模式的 I/O |

---

## 集成指南

### 前置依赖
- Qt 5.15+ 或 Qt 6.x（支持信号/槽机制）
- C++17 或更高（枚举类、auto 类型推导）

### 编译步骤
```bash
cd /path/to/antimicrox-xbox-controll
mkdir -p build && cd build
cmake ..
make
```

### 运行时注意事项
1. **配置文件兼容性：** 旧版本配置文件会被自动迁移（新增属性使用默认值）
2. **线程安全：** JoyButton 的标签修改应在主线程进行
3. **内存管理：** JoyButtonWidget 拥有 JoyButton 的指针，生命周期由 Qt 管理

---

## 测试检查清单

- [ ] 创建新按钮，验证默认显示为 MappingMode
- [ ] 设置自定义标签（<100 字符），验证 UI 更新
- [ ] 切换到 FunctionMode，验证显示自定义标签
- [ ] 切换回 MappingMode，验证显示映射名
- [ ] 删除标签，验证显示回退到映射名
- [ ] 保存配置文件，关闭应用
- [ ] 重新打开应用，验证标签和模式被正确恢复
- [ ] 测试标签长度边界（99、100、101 字符）
- [ ] 验证 XML 格式的正确性

---

## 已知限制与未来改进

### 当前限制
1. 标签最多 100 字符（可按需调整）
2. 仅支持两种显示模式（映射 vs 功能）
3. 标签修改需通过代码或配置文件，暂无 UI 编辑界面

### 建议的下一步工作
1. **UI 编辑对话框**
   - 在按钮右键菜单中添加"编辑标签"选项
   - 提供文本输入框，实时验证长度
   
2. **扩展显示模式**
   - 混合模式：显示 "标签 (映射)"
   - 简化模式：显示简短的功能代码
   
3. **标签预设库**
   - 常见游戏的标准按钮标签集合
   - 一键应用完整预设

4. **国际化支持**
   - 多语言标签存储
   - 根据应用语言自动切换

---

## 联系与文档

- **仓库：** https://github.com/yinparafly/antimicrox-xbox-controll
- **原始项目：** https://github.com/AntiMicroX/antimicrox
- **许可证：** GNU General Public License v3.0

---

## 变更日志

**v1.0 - 初始版本**
- ✅ 实现自定义功能标签功能
- ✅ 实现显示模式切换
- ✅ 实现 XML 持久化
- ✅ 完成信号驱动 UI 更新机制
