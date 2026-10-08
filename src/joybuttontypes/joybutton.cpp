/* antimicrox Gamepad to KB+M event mapper
 * Copyright (C) 2015 Travis Nickles <nickles.travis@gmail.com>
 * Copyright (C) 2020 Jagoda Górska <juliagoda.pl@protonmail>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "joybutton.h"

#include "event.h"
#include "inputdevice.h"
#include "logger.h"
#include "setjoystick.h"
#include "vdpad.h"

#include "SDL2/SDL_events.h"
#include "eventhandlerfactory.h"

#include <QDebug>
//#include <QThread>
#include <QSharedPointer>
#include <QStringList>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QtConcurrent>
#include <chrono>

const JoyButton::JoyMouseCurve JoyButton::DEFAULTMOUSECURVE = JoyButton::EnhancedPrecisionCurve;
const JoyButton::SetChangeCondition JoyButton::DEFAULTSETCONDITION = JoyButton::SetChangeDisabled;
const JoyButton::JoyMouseMovementMode JoyButton::DEFAULTMOUSEMODE = JoyButton::MouseCursor;
const JoyButton::TurboMode JoyButton::DEFAULTTURBOMODE = JoyButton::NormalTurbo;
const JoyButton::JoyExtraAccelerationCurve JoyButton::DEFAULTEXTRAACCELCURVE = JoyButton::LinearAccelCurve;
const JoyButton::DisplayMode JoyButton::DEFAULTDISPLAYMODE = JoyButton::MappingMode;

JoyButtonSlot *JoyButton::lastActiveKey = nullptr;

// Keep track of active Mouse Speed Mod slots.
QList<JoyButtonSlot *> JoyButton::mouseSpeedModList;

// Lists used for cursor mode calculations.
QList<JoyButton::mouseCursorInfo> JoyButton::cursorXSpeeds;
QList<JoyButton::mouseCursorInfo> JoyButton::cursorYSpeeds;

// Lists used for spring mode calculations.
QList<PadderCommon::springModeInfo> JoyButton::springXSpeeds;
QList<PadderCommon::springModeInfo> JoyButton::springYSpeeds;

// Temporary test object to test old mouse time behavior.
QElapsedTimer JoyButton::testOldMouseTime;

// time when minislots next to each other in thread pool are waiting to execute function
// at the same time
int JoyButton::timeBetweenMiniSlots = 55;

int JoyButton::allSlotTimeBetweenSlots = 0;

// Helper object to have a single mouse event for all JoyButton
// instances.
JoyButtonMouseHelper JoyButton::mouseHelper;

QTimer JoyButton::staticMouseEventTimer;
QList<JoyButton *> JoyButton::pendingMouseButtons;

// IT CAN BE HERE
// LOOK FOR JoyCycle and put JoyMix next to the slots types
JoyButton::JoyButton(int sdl_button_index, int originset, SetJoystick *parentSet, QObject *parent)
    : QObject(parent)
{
    m_vdpad = nullptr;
    slotiter = nullptr;

    threadPool = QThreadPool::globalInstance();

    turboTimer.setParent(this);
    pauseTimer.setParent(this);
    holdTimer.setParent(this);
    pauseWaitTimer.setParent(this);
    createDeskTimer.setParent(this);
    releaseDeskTimer.setParent(this);
    mouseWheelVerticalEventTimer.setParent(this);
    mouseWheelHorizontalEventTimer.setParent(this);
    setChangeTimer.setParent(this);
    keyPressTimer.setParent(this);
    delayTimer.setParent(this);
    slotSetChangeTimer.setParent(this);
    setChangeTimer.setSingleShot(true);
    slotSetChangeTimer.setSingleShot(true);
    m_parentSet = parentSet;

    connect(&pauseWaitTimer, &QTimer::timeout, this, &JoyButton::pauseWaitEvent);
    connect(&keyPressTimer, &QTimer::timeout, this, &JoyButton::keyPressEvent);
    connect(&holdTimer, &QTimer::timeout, this, &JoyButton::holdEvent);
    connect(&delayTimer, &QTimer::timeout, this, &JoyButton::delayEvent);
    connect(&createDeskTimer, &QTimer::timeout, this, &JoyButton::waitForDeskEvent);
    connect(&releaseDeskTimer, &QTimer::timeout, this, &JoyButton::waitForReleaseDeskEvent);
    connect(&turboTimer, &QTimer::timeout, this, &JoyButton::turboEvent);
    connect(&mouseWheelVerticalEventTimer, &QTimer::timeout, this, &JoyButton::wheelEventVertical);
    connect(&mouseWheelHorizontalEventTimer, &QTimer::timeout, this, &JoyButton::wheelEventHorizontal);
    connect(&setChangeTimer, &QTimer::timeout, this, &JoyButton::checkForSetChange);
    connect(&slotSetChangeTimer, &QTimer::timeout, this, &JoyButton::slotSetChange);

    // Will only matter on the first call
    establishMouseTimerConnections();

    // Make sure to call before calling reset
    resetAllProperties();

    m_index_sdl = sdl_button_index;
    m_originset = originset;
    quitEvent = true;
    DEBUG() << "Created button with ID: " << m_index_sdl << " For set: " << originset << " Name: " << getName();
}

JoyButton::~JoyButton()
{ // threadPool->clear();

    reset();
    // resetPrivVars();
}

void JoyButton::setFunctionLabel(QString label)
{
    if ((label.length() <= 100) && (label != functionLabel))
    {
        functionLabel = label;
        emit functionLabelChanged();
        emit propertyUpdated();
    }
}

void JoyButton::setDisplayMode(DisplayMode mode)
{
    if (mode != displayMode)
    {
        displayMode = mode;
        emit displayModeChanged();
        emit propertyUpdated();
    }
}

QString JoyButton::getFunctionLabel() const
{
    return functionLabel;
}

JoyButton::DisplayMode JoyButton::getDisplayMode() const
{
    return displayMode;
}
