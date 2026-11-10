#include "worker.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <unistd.h>

Worker::Worker(QObject *parent) : QObject(parent) {
    m_state = {{"connected", false}, {"busy", false}, {"printing", false},
               {"message", tr("Not connected")}, {"progress", 0}, {"prompt", QString()}};
}

void Worker::initialize() {
    m_timer = new QTimer(this);
    m_timer->setInterval(30);
    connect(m_timer, &QTimer::timeout, this, &Worker::tick);
    m_timer->start();
}

void Worker::publish() { emit updated(m_state); }

void Worker::fail(const QString &message) {
    m_state["message"] = message;
    m_state["error"] = true;
    publish();
}

bool Worker::prepareConversion(const QVariantMap &args) {
    const QString input = args.value("input").toString();
    m_output = args.value("output").toString();
    const QByteArray path = QFile::encodeName(input);
    if (!QFileInfo(input).isFile() || path.size() >= MAX_PATH ||
        (!m_xyz.isGcodeFile(path.constData()) && !m_xyz.is3wFile(path.constData()))) {
        fail(tr("Choose an existing .gcode or .3w input with a path shorter than 260 bytes."));
        return false;
    }
    if (m_output.isEmpty() || QFileInfo::exists(m_output) || QFileInfo(m_output).isSymLink()) {
        fail(tr("Choose a new output file. Existing files are never overwritten."));
        return false;
    }
    const int model = args.value("model", -1).toInt();
        if (m_xyz.isGcodeFile(path.constData()) && !XYZV3::indexToInfo(model)) {
        fail(tr("Select the target printer model before converting G-code."));
        return false;
    }
    m_temporary = std::make_unique<QTemporaryDir>(QFileInfo(m_output).absolutePath() + "/.minimover-XXXXXX");
    m_temporaryFile = m_temporary->filePath("converted");
    if (!m_temporary->isValid() || QFile::encodeName(m_temporaryFile).size() >= MAX_PATH) {
        m_temporary.reset();
        fail(tr("Cannot create a temporary output file. Check the folder and permissions."));
        return false;
    }
    m_xyz.convertFileStart(path.constData(), QFile::encodeName(m_temporaryFile).constData(), model);
    return true;
}

bool Worker::commitConversion() {
    // Both paths share a filesystem. Linking the completed temporary file
    // publishes it atomically and cannot replace a file created concurrently.
    return ::link(QFile::encodeName(m_temporaryFile).constData(), QFile::encodeName(m_output).constData()) == 0;
}

void Worker::command(const QString &action, const QVariantMap &args) {
    if (action == "continue" && m_state["busy"].toBool()) {
        const QString prompt = m_state["prompt"].toString();
        if (prompt == "lower") m_xyz.calibrateBedDetectorLowered();
        else if (prompt == "raise") m_xyz.calibrateBedDetectorRaised();
        else if (prompt == "clean") m_xyz.cleanNozzleCancel();
        else if (prompt == "load") m_xyz.loadFilamentCancel();
        m_state["prompt"] = QString();
        publish();
        return;
    }
    if (m_state["busy"].toBool()) {
        fail(tr("Wait for the current operation to finish."));
        return;
    }
    m_state["error"] = false;
    if (action == "disconnect") {
        m_xyz.setStream(nullptr);
        m_state["connected"] = false;
        m_state["printing"] = false;
        m_state["message"] = tr("Disconnected. A running print is not canceled.");
        publish();
        return;
    }
    if (action == "connect") {
        m_xyz.setStream(nullptr);
        m_state["connected"] = false;
        QString device = args.value("device").toString().trimmed();
        if (device.isEmpty()) {
            const char *found = SerialHelper::getPortDeviceName(SerialHelper::queryForPorts("XYZ"));
            if (found) device = QString::fromLocal8Bit(found);
        }
        const auto encoded = QFile::encodeName(device);
        const bool network = args.value("network").toBool();
        const bool opened = !device.isEmpty() && (network
            ? m_socket.openStream(encoded.constData(), args.value("port", 9100).toInt())
            : m_serial.openStream(encoded.constData(), 115200));
        if (!opened) {
            fail(tr("Cannot open the printer. Check its address, USB cable, and device permissions."));
            return;
        }
        m_xyz.setStream(network ? static_cast<StreamT *>(&m_socket) : &m_serial);
        m_xyz.queryStatusStart();
    } else if (action == "convert") {
        if (!prepareConversion(args)) return;
    } else {
        if (!m_state["connected"].toBool()) { fail(tr("Connect to a printer first.")); return; }
        const bool printing = m_state["printing"].toBool();
        if (action == "pause" || action == "resume" || action == "cancel") {
            if (!printing) { fail(tr("No print is being monitored.")); return; }
            if (action == "pause") m_xyz.pausePrint();
            else if (action == "resume") m_xyz.resumePrint();
            else m_xyz.cancelPrint();
            m_state["message"] = tr("Command sent. Refresh status to verify the printer response.");
            publish();
            return;
        }
        if (printing && action != "status") { fail(tr("Printer controls are locked while printing.")); return; }
        const auto *info = m_xyz.getPrinterInfo();
        if (action != "print" && action != "status" && (!info || info->hasTuningMenu)) {
            fail(tr("Use the printer's LCD tuning menu for maintenance controls.")); return;
        }
        if (action == "status") m_xyz.queryStatusStart();
        else if (action == "print") {
            const QString path = args.value("input").toString();
            const auto encoded = QFile::encodeName(path);
            if (!QFileInfo(path).isFile() || encoded.size() >= MAX_PATH ||
                (!m_xyz.isGcodeFile(encoded.constData()) && !m_xyz.is3wFile(encoded.constData()))) {
                fail(tr("Choose an existing .gcode or .3w print file.")); return;
            }
            m_xyz.printFileStart(encoded.constData());
        } else if (action == "home") m_xyz.homePrinterStart();
        else if (action == "jog") {
            const QString axis = args.value("axis").toString();
            const int distance = args.value("distance").toInt();
            if ((axis != "x" && axis != "y" && axis != "z") || distance == 0 || distance < -10 || distance > 10) {
                fail(tr("Jog distance must be between -10 and 10 mm on X, Y, or Z.")); return;
            }
            m_xyz.jogPrinterStart(axis.at(0).toLatin1(), distance);
        } else if (action == "load") m_xyz.loadFilamentStart();
        else if (action == "unload") m_xyz.unloadFilamentStart();
        else if (action == "calibrate") m_xyz.calibrateBedStart();
        else if (action == "clean") m_xyz.cleanNozzleStart();
        else if (action == "buzzer") m_xyz.setBuzzerStart(args.value("enabled").toBool());
        else if (action == "level") m_xyz.setAutoLevelStart(args.value("enabled").toBool());
        else { fail(tr("Unknown command: %1").arg(action)); return; }
    }
    m_action = action;
    m_state["busy"] = true;
    m_state["prompt"] = QString();
    m_state["progress"] = 0;
    m_state["message"] = tr("Working…");
    publish();
}

void Worker::refreshStatus() {
    const auto *status = m_xyz.getPrinterStatus();
    if (!status->isValid) return;
    const auto *info = m_xyz.getPrinterInfo();
    m_state["model"] = info ? QString::fromUtf8(info->screenName) : QString::fromUtf8(status->pMachineModelNumber);
    m_state["printerStatus"] = QString::fromUtf8(status->jPrinterStateStr);
    m_state["temperature"] = status->tExtruder1ActualTemp_C;
    m_state["targetTemperature"] = status->tExtruderTargetTemp_C;
    m_state["filament"] = status->fFilament1Remaining_mm / 1000.0;
    m_state["printProgress"] = status->dPrintPercentComplete;
    m_state["printerError"] = QString::fromUtf8(status->eErrorStatusStr);
    m_state["printing"] = status->jPrinterState != PRINT_NONE &&
        status->jPrinterState != PRINT_ENDING_PROCESS_DONE && status->jPrinterState != PRINT_JOB_DONE;
    m_state["hasMenu"] = !info || info->hasTuningMenu;
}

void Worker::tick() {
    if (m_state["busy"].toBool()) {
        m_xyz.doProcess();
        m_state["progress"] = qBound(0, m_xyz.getProgressPct(), 100);
        QString prompt;
        if (m_action == "calibrate" && m_xyz.calibrateBedPromptToLowerDetector()) prompt = "lower";
        else if (m_action == "calibrate" && m_xyz.calibrateBedPromptToRaiseDetector()) prompt = "raise";
        else if (m_action == "clean" && m_xyz.cleanNozzlePromtToClean()) prompt = "clean";
        else if (m_action == "load" && m_xyz.loadFilamentPromptToFinish()) prompt = "load";
        m_state["prompt"] = prompt;
        if (!m_xyz.isInProgress()) {
            bool success = m_xyz.isSuccess();
            if (m_action == "convert" && success) success = commitConversion();
            m_temporary.reset();
            if (m_action == "connect") {
                success = success && m_xyz.getPrinterStatus()->isValid && m_xyz.getPrinterInfo();
                m_state["connected"] = success;
                if (!success) m_xyz.setStream(nullptr);
            }
            if (success && (m_action == "status" || m_action == "connect")) refreshStatus();
            if (!success && m_action == "status") {
                m_state["connected"] = false;
                m_xyz.setStream(nullptr);
            }
            if (success && m_action == "print") m_state["printing"] = true;
            m_state["busy"] = false;
            m_state["prompt"] = QString();
            m_state["error"] = !success;
            m_state["message"] = success ? tr("Operation completed.") : tr("Operation failed. Check the printer, connection, and selected file.");
            m_lastPoll = QDateTime::currentMSecsSinceEpoch();
        }
        publish();
    } else if (m_state["connected"].toBool() && QDateTime::currentMSecsSinceEpoch() - m_lastPoll > 5000) {
        command("status");
    }
}
