#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "modbus-tcp.h"
#include "modbus.h"

#include <QtCharts>
#include <QChartView>
#include <QIntValidator>
#include <QSettings>
#include <QScrollBar>
#include <QDebug>
#include <errno.h>

const int DataTypeColumn = 0;
const int AddrColumn = 1;
const int DataColumn = 2;
const int DatanumColumn = 3;
const int NoteColumn = 4;

int LTval;
int RTval;
int asdf;

extern MainWindow * globalMainWin;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow),
    m_tcpModbus(NULL),
    m_tcpActive(false),
    m_poll(false)
{
    ui->setupUi(this);

    connect(&GamepadServer::instance(), SIGNAL(stateUpdate(GamepadState, int)),
            this, SLOT(catchGamepadState(GamepadState, int)));

    refreshRate = new QTimer(this);
    connect(refreshRate, SIGNAL(timeout()), this, SLOT(sendTcpRequest()));

    series = new QLineSeries();
    series1 = new QLineSeries();
    series2 = new QLineSeries();
    chart = new QChart();

    chart->addSeries(series);
    chart->addSeries(series1);
    chart->addSeries(series2);

    //chart->legend()->hide();
    series->name();

    series->setPointLabelsVisible(true);    // is false by default
    series->setPointLabelsColor(Qt::white);
    series->setPointLabelsFormat("@yPoint");

    series1->setPointLabelsVisible(true);    // is false by default
    series1->setPointLabelsColor(Qt::white);
    series1->setPointLabelsFormat("@yPoint");

    series2->setPointLabelsVisible(true);    // is false by default
    series2->setPointLabelsColor(Qt::white);
    series2->setPointLabelsFormat("@yPoint");

    chart->createDefaultAxes();
    chart->axisX()->setRange(Xmin, Xmax);
    chart->axisY()->setRange(Ymin, Ymax);

    chart->setBackgroundVisible(false);
    chart->setMargins(QMargins(0,0,0,0));
    chart->layout()->setContentsMargins(0,0,0,0);
    chart->setPlotAreaBackgroundBrush(QBrush(Qt::black));
    chart->setPlotAreaBackgroundVisible(true);
    chartView = new QChartView(chart);
    ui->gridLayout->addWidget(chartView);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::catchGamepadState(const GamepadState & gps, const int & playerId) {
    qDebug() << "Player " << playerId << ": ";

    qDebug() << "Left Trigger: " << gps.m_lTrigger <<
        "\tRight Trigger: " << gps.m_rTrigger;
    qDebug() << "Left Thumb :: X Axis: " << gps.m_lThumb.xAxis <<
        "\t Y Axis: " << gps.m_lThumb.yAxis;
    qDebug() << "Right Thumb :: Y Axis: " << gps.m_rThumb.xAxis <<
        "\t Y Axis: " << gps.m_rThumb.yAxis;

    LTval = gps.m_lTrigger;
    RTval = gps.m_rTrigger;

    if (gps.m_pad_a) {
        qDebug() << "A Pressed.";
    }
    if (gps.m_pad_b) {
        qDebug() << "B Pressed.";
    }
    if (gps.m_pad_x) {
        qDebug() << "X Pressed.";
    }
    if (gps.m_pad_y) {
        qDebug() << "Y Pressed.";
    }
    if (gps.m_pad_up) {
        qDebug() << "Up Pressed.";
    }
    if (gps.m_pad_down) {
        qDebug() << "Down Pressed.";
    }
    if (gps.m_pad_left) {
        qDebug() << "Left Pressed.";
    }
    if (gps.m_pad_right) {
        qDebug() << "Right Pressed.";
    }
    if (gps.m_lShoulder) {
        qDebug() << "Left Shoulder Pressed.";
    }
    if (gps.m_rShoulder) {
        qDebug() << "Right Shoulder Pressed.";
    }
    if (gps.m_lThumb.pressed) {
        qDebug() << "Left Thumb Pressed.";
    }
    if (gps.m_rThumb.pressed) {
        qDebug() << "Right Thumb Pressed.";
    }
    if (gps.m_pad_start) {
        qDebug() << "Start Pressed.";
    }
    if (gps.m_pad_back) {
        qDebug() << "Back Pressed.";
    }

}


void MainWindow::on_tcpEnable_clicked(bool checked)
{
    enableTCPEdit(true);
    if(checked)
    {
        refreshRate->setInterval(ui->refreshRateEdit->text().toInt());
        refreshRate->start();
        m_tcpActive = true;
        enableTCPEdit(false);
    }
    else
    {
        refreshRate->stop();
        enableTCPEdit(false);
    }
}


void MainWindow::enableTCPEdit(bool checked)
{
    ui->edNetworkAddress->setEnabled(checked);
    ui->edPort->setEnabled(checked);
    ui->refreshRateEdit->setEnabled(checked);
}

void MainWindow::tcpConnect()
{
    int portNbr = ui->edPort->text().toInt();
    changeModbusInterface(ui->edNetworkAddress->text(), portNbr);
}

int MainWindow::setupModbusPort()
{
    return 0;
}

static QString descriptiveDataTypeName( int funcCode )
{
    switch( funcCode )
    {
    case MODBUS_FC_READ_COILS:
    case MODBUS_FC_WRITE_SINGLE_COIL:
    case MODBUS_FC_WRITE_MULTIPLE_COILS:
        return "Coil (binary)";
    case MODBUS_FC_READ_DISCRETE_INPUTS:
        return "Discrete Input (binary)";
    case MODBUS_FC_READ_HOLDING_REGISTERS:
    case MODBUS_FC_WRITE_SINGLE_REGISTER:
    case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
        return "Holding Register (16 bit)";
    case MODBUS_FC_READ_INPUT_REGISTERS:
        return "Input Register (16 bit)";
    default:
        break;
    }
    return "Unknown";
}


void MainWindow::changeModbusInterface(const QString& address, int portNbr )
{
    if (m_tcpModbus)
    {
        modbus_close(m_tcpModbus);
        modbus_free(m_tcpModbus);
        m_tcpModbus = NULL;
    }

    m_tcpModbus = modbus_new_tcp( address.toLatin1().constData(), portNbr);
    if(modbus_connect(m_tcpModbus) == -1)
    {
        if (m_tcpModbus)
        {
            emit connectionError(tr ("Could not connect to TCP/IP Port!"));

            modbus_close(m_tcpModbus);
            modbus_free(m_tcpModbus);
            m_tcpModbus = NULL;
        }
    }

}

void MainWindow::sendTcpRequest(void)
{

    if(m_tcpActive)
    {
        tcpConnect();
    }
    if(m_tcpModbus == NULL)
    {
        ui->connectionStatus->setText("Not Connected");
        return;
    }

    ui->connectionStatus->setText("Connection Established");
    const int slave = ui->slaveId->value();
    const int func = 04;
    const int addr = ui->addressReg->value();
    int num = ui->coilNum->value();
    uint8_t dest[1024];
    uint8_t dest_2[1024];
    uint16_t * dest16 = (uint16_t *) dest;
    uint16_t * dest16_2 = (uint16_t *) dest_2;

    memset(dest, 0, 1024);
    memset(dest_2, 0 ,1024);

    int ret = -1;
    bool is16Bit = false;
    const QString dataType = descriptiveDataTypeName(func);
    QString noteValue = "";

    modbus_set_slave(m_tcpModbus, slave);

    ret = modbus_read_registers( m_tcpModbus, addr, num, dest16);
    is16Bit = true;
    //ui->debugLabel->setText();
    //qDebug() << ret;

    asdf = modbus_write_register(m_tcpModbus, 9, 123);
    qDebug() << asdf;

    if(ret == num)
    {
        bool b_hex = is16Bit;
        QString qs_num;
        ui->regTable->setRowCount( num );

        for(int i = 0; i < num; ++i)
        {
            spinVal = ui->spinBox->value();
            Xmin = plotcount - spinVal;

            int dataraw = is16Bit ? dest16[i] : dest[i];
            int data;
            if(dataraw >= 32768){
                data = dataraw - 65536;
            } else {
                data = dataraw;
            }

            QTableWidgetItem * dtItem =
                new QTableWidgetItem( dataType );
            QTableWidgetItem * addrItem =
                new QTableWidgetItem ( QString::number( addr+i ) );
            qs_num = QString::asprintf( b_hex ? "0x%04x" : "%d", data);
            QTableWidgetItem * dataItem =
                new QTableWidgetItem( qs_num );
            QTableWidgetItem * datanumItem =
                new QTableWidgetItem( QString::number(data) );

            switch (i){
            default:
                noteValue = "-";
                break;
            case 0:
                noteValue = "angleSample";
                break;
            case 1:
                noteValue = "finalSpeed";
                break;
            case 2:
                noteValue = "targetSpeed";
                break;
            case 3:
                noteValue = "targetAngle";
                break;
            case 4:
                noteValue = "INIT_SAMPLE_SIZE";
                break;
            case 5:
                noteValue = "CENTER_OF_MASS_OFFSET";
                break;
            case 6:
                noteValue = "KP";
                break;
            case 7:
                noteValue = "KI";
                break;
            case 8:
                noteValue = "KD";
                break;
            case 9:
                noteValue = "LEFT-TRIG";
                break;
            case 10:
                noteValue = "RIGHT-TRIG";
                break;
            }

            QTableWidgetItem * noteItem =
                new QTableWidgetItem(noteValue);

            dtItem->setFlags(dtItem->flags() &
                             ~Qt::ItemIsEditable);
            addrItem->setFlags(addrItem->flags() &
                             ~Qt::ItemIsEditable);
            dataItem->setFlags( dataItem->flags() &
                              ~Qt::ItemIsEditable);
            datanumItem->setFlags( datanumItem->flags() &
                               ~Qt::ItemIsEditable);
            noteItem->setFlags( noteItem->flags() &
                               ~Qt::ItemIsEditable);

            ui->regTable->setItem(i, DataTypeColumn, dtItem);
            ui->regTable->setItem(i, AddrColumn, addrItem);
            ui->regTable->setItem(i, DataColumn, dataItem);
            ui->regTable->setItem(i, DatanumColumn, datanumItem);
            ui->regTable->setItem(i, NoteColumn, noteItem);

            //double val = 3*(qSin((double)plotcount*2)+2); //for testing
            switch (i) {
            case 0:
                series->append(plotcount, data);
                plotcount++;
                if (plotcount > Xmax){
                    Xmin++;
                    Xmax = plotcount;
                }
                // if (data >= Ymax){
                //     Ymax = (data + 10);
                // }
                chart->axisX()->setRange(Xmin, Xmax-1);
                // chart->axisY()->setRange(Ymin, Ymax);
                break;
            case 1:
                series1->append(plotcount, data);
                plotcount++;
                if (plotcount > Xmax){
                    Xmin++;
                    Xmax = plotcount;
                }
                // if (data >= Ymax){
                //     Ymax = (data + 10);
                // }
                chart->axisX()->setRange(Xmin, Xmax-1);
                // chart->axisY()->setRange(Ymin, Ymax);
                break;
            case 2:
                series2->append(plotcount, data);
                plotcount++;
                if (plotcount > Xmax){
                    Xmin++;
                    Xmax = plotcount;
                }
                // if (data >= Ymax){
                //     Ymax = (data + 10);
                // }
                chart->axisX()->setRange(Xmin, Xmax-1);
                // chart->axisY()->setRange(Ymin, Ymax);
            default:
                break;
            }


        }

        ui->regTable->resizeColumnToContents(0);
        ui->debugLabel->setText("Refresh Success!");
    }
    else
    {
    ui->debugLabel->setText("Refresh Error!");
    }

}


void MainWindow::setStatusError(const QString &msg)
{
    m_statusText->setText( msg );
    m_statusInd->setStyleSheet("background: red");
    m_statusTimer->start( 2000 );
}

void MainWindow::on_pushButton_15_clicked()
{
    // double val = 3*(qSin((double)plotcount*2)+2);
    // series->append(plotcount,val); // Enough to trigger repaint!
    // plotcount++;
    // if (plotcount > Xmax){
    //     Xmin++;
    //     Xmax++;
    // }
    // chart->axisX()->setRange(Xmin, Xmax-1);
}

