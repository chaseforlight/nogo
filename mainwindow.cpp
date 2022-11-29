#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPainter>
#include <QMouseEvent>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("不围棋"); //设置窗口标题

    //利用gamemodel里定义的常量计算窗口大小
    W = MARGIN * 2 + (LINE_NUM - 1) * LINE_DIST;
    H = MARGIN * 2 + (LINE_NUM - 1) * LINE_DIST;
    setFixedSize(W,H); //为窗口设置固定大小
    linex = -1; //默认-1，即非合法位置
    liney = -1;
    pointSelected = false;
    latestx = -1;
    latesty = -1;
    stepCount = 0;
    isSaved = true;
    remainingTime = TIME_LIMIT;

    setMouseTracking(true); //让窗口始终跟踪鼠标位置

    on_actionShowGameInfo_triggered(); //游戏开始时，呼出提示窗口

    game = new GameModel;

    labelGameType = new QLabel;
    if(game->getType() == PVP)
        labelGameType->setText("当前模式：PVP");
    else if(game->getType() == PVE)
        labelGameType->setText("当前模式：PVE");
    ui->statusbar->addWidget(labelGameType);

    labelSteps = new QLabel;
    labelSteps->setText(QString::asprintf("    当前步数:%d",stepCount));
    ui->statusbar->addWidget(labelSteps);

    labelTimer = new QLabel;
    labelTimer->setText(QString::asprintf("    剩余时间:%d秒",remainingTime));
    ui->statusbar->addWidget(labelTimer);

    timer = new QTimer;
    timer->setInterval(1000);//设置timer发送timeout信号的时间间隔为1000ms
    timer->start();

    sound = new QSound(":/res/music/placechess.wav",this);
    canPlayMusic = true; //默认开启音效
    ui->actionPlaceChessMusic->setChecked(true); //默认勾选开启落子提示音选项

    connect(timer,SIGNAL(timeout()),this,SLOT(on_timeOut()));
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing); //设置抗锯齿

    //画背景图片
    QPixmap pixmap(":/res2/images/background.png");
    painter.drawPixmap(this->rect(),pixmap);

    //画棋盘
    for(int i = 0;i < LINE_NUM;i++){
        //画竖线
        painter.drawLine(MARGIN + i * LINE_DIST,MARGIN,MARGIN + i * LINE_DIST,MARGIN + (LINE_NUM - 1) * LINE_DIST);
        //画横线
        painter.drawLine(MARGIN,MARGIN + i * LINE_DIST,MARGIN + (LINE_NUM - 1) * LINE_DIST,MARGIN + i * LINE_DIST);
    }

    //画提示点
    if(pointSelected){
        //设置并应用brush
        QBrush brush;
        brush.setStyle(Qt::SolidPattern);
        if(game->getPlayer() == Black) brush.setColor(Qt::black);
        else brush.setColor(Qt::white);
        painter.setBrush(brush);
        //计算出应画点的位置
        int centerx = MARGIN + (linex - 1) * LINE_DIST;
        int centery = MARGIN + (liney - 1) * LINE_DIST;
        QPoint center(centerx,centery);
        //画点
        painter.drawEllipse(center,DOT_DIAM/2,DOT_DIAM/2);
    }

    //画棋子
    //画棋子应遍历整个棋盘，为有子点画上棋子，不能像上面画提示点那样画，否则下一次刷新时，棋子就没了
    QBrush brush;
    brush.setStyle(Qt::SolidPattern);
    for(int i = 1;i <= LINE_NUM;i++){
        for(int j = 1;j <= LINE_NUM;j++){
            int centerx = MARGIN + (i - 1) * LINE_DIST;
            int centery = MARGIN + (j - 1) * LINE_DIST;
            QPoint center(centerx,centery);
            if(game->getBoard(i,j) == 1){
                brush.setColor(Qt::black);
                painter.setBrush(brush);
                painter.drawEllipse(center,CHESS_DIAM/2,CHESS_DIAM/2);
            }else if(game->getBoard(i,j) == 2){
                brush.setColor(Qt::white);
                painter.setBrush(brush);
                painter.drawEllipse(center,CHESS_DIAM/2,CHESS_DIAM/2);
            }
        }
    }

    //画最后一步棋的提示红点
    if(latestx != -1){
        QBrush brush;
        brush.setStyle(Qt::SolidPattern);
        brush.setColor(Qt::red);
        painter.setBrush(brush);
        int centerx = MARGIN + (latestx - 1) * LINE_DIST;
        int centery = MARGIN + (latesty - 1) * LINE_DIST;
        QPoint center(centerx,centery);
        painter.drawEllipse(center,DOT_DIAM/2,DOT_DIAM/2);
    }

    //更新显示步数的label
    labelSteps->setText(QString::asprintf("    当前步数:%d",stepCount));

    //显示禁手
    QPen pen;
    pen.setColor(Qt::blue);
    pen.setWidth(3);
    painter.setPen(pen);
    if(ui->actionShowLosingPoints->isChecked()){
        for(int i = 1;i <= LINE_NUM;i++){
            for(int j = 1;j <= LINE_NUM;j++){
                if(game->getBoard(i,j) != 0) continue;
                if(game->isAbleToPlaceChess(i,j,game->getPlayer()) == false){
                    int centerx = MARGIN + (i - 1) * LINE_DIST;
                    int centery = MARGIN + (j - 1) * LINE_DIST;
                    QPoint center(centerx,centery);
                    painter.drawLine(centerx+5,centery+5,centerx-5,centery-5);
                    painter.drawLine(centerx+5,centery-5,centerx-5,centery+5);
                }
            }
        }
    }
    //设置悔棋按键是否可点击
    ui->actionRetract->setEnabled(stepCount != 0 && game->getState() == Playing);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if(game->getState() == GameOver) return;
    //获取鼠标的坐标
    QPoint point = event->pos();
    int x = point.x();
    int y = point.y();
    //获取鼠标坐标横向和竖向相距最近的线的标号
    int lx = (x - MARGIN) / LINE_DIST + 1;
    int ly = (y - MARGIN) / LINE_DIST + 1;
    if(x - (MARGIN + (lx - 1) * LINE_DIST) > LINE_DIST / 2) lx++;
    if(y - (MARGIN + (ly - 1) * LINE_DIST) > LINE_DIST / 2) ly++;
    //获取邻近点的x,y坐标
    int px = MARGIN + (lx - 1) * LINE_DIST;
    int py = MARGIN + (ly - 1) * LINE_DIST;
    //判断该点是否合法
    if(lx > 0 && lx < LINE_NUM+1 && ly > 0 && ly < LINE_NUM+1 &&
            game->getBoard(lx,ly) == 0){
        //判断该点是否在合理距离内
        if((x-px)*(x-px) + (y-py)*(y-py) <= (SELECTED_DIAM/2)*(SELECTED_DIAM/2)){
            pointSelected = true;
            linex = lx;
            liney = ly;
        }
        else{
            pointSelected = false;
            linex = -1;
            liney = -1;
        }
    }
    else{
        pointSelected = false;
        linex = -1;
        liney = -1;
    }
    update(); //触发paintEvent事件
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    //在鼠标点下并松开后，再实现落子效果

    Q_UNUSED(event);
    if(game->getState() == GameOver) return;

    if(pointSelected == true){
        //将pointSelected重置为false,以防止鼠标未移动(即不会更新pointSelected)而在同一点连续落子
        pointSelected = false;

        if(game->getType() == PVE){
            if(canPlayMusic) sound->play(); //播放落子音效

            game->changeBoard(linex,liney,game->getPlayer());//修改board,已实现落子
            latestx = linex;
            latesty = liney;
            stepCount++;
            steps[stepCount][0] = latestx;
            steps[stepCount][1] = latesty;
            update();
            game->updateQi(linex,liney,game->getPlayer());
            game->checkWin(game->getPlayer());

            timer->stop();
            remainingTime = TIME_LIMIT;

            if(game->getState() == GameOver) return;

            EvaluateValue ev = game->miniMax(-1000,1000,1,4);
            //qDebug() << '(' << ev.lx << ',' << ev.ly << ')' << "   " << ev.value;
            if(ev.lx == -1){
                //如果AI分析后传出来的点为(-1,-1),说明AI已无路可走，即黑方获胜
                game->onWin(2);
                return;
            }
            game->changeBoard(ev.lx,ev.ly,2);
            latestx = ev.lx;
            latesty = ev.ly;
            stepCount++;
            steps[stepCount][0] = latestx;
            steps[stepCount][1] = latesty;
            update();
            game->updateQi(ev.lx,ev.ly,2);
            game->checkWin(2);

            timer->start();

            isSaved = false;
        }
        else if(game->getType() == PVP){
            if(canPlayMusic) sound->play(); //播放落子音效

            game->changeBoard(linex,liney,game->getPlayer());//修改board,已实现落子
            latestx = linex;
            latesty = liney;
            stepCount++;
            steps[stepCount][0] = latestx;
            steps[stepCount][1] = latesty;
            update();
            game->updateQi(linex,liney,game->getPlayer());
            game->checkWin(game->getPlayer());

            remainingTime = TIME_LIMIT;

            //交换player
            if(game->getPlayer() == Black) game->changePlayer(White);
            else game->changePlayer(Black);

            isSaved = false;
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    //关闭窗口时，若当前棋局未结束且未保存，将弹出提示窗口
    if(game->getState() == Playing && isSaved == false){
        QMessageBox msgBox;
        msgBox.setWindowTitle("提示");
        msgBox.setInformativeText("当前棋局未结束且未保存，确认退出？");
        msgBox.setStandardButtons(QMessageBox::Cancel|QMessageBox::Ok);
        msgBox.setButtonText(QMessageBox::Cancel,"返回");
        msgBox.setButtonText(QMessageBox::Ok,"仍然退出");
        msgBox.setDefaultButton(QMessageBox::Ok);
        int ret = msgBox.exec();
        if(ret == QMessageBox::Ok){
            event->accept();
        }else{
            event->ignore();
        }
    }
}



void MainWindow::on_actionPVP_triggered()
{
    delete game;
    game = new GameModel;
    game->changeGameType(PVP);
    labelGameType->setText("当前模式：PVP");
    latestx = latesty = -1;
    stepCount = 0;
    remainingTime = TIME_LIMIT;
    timer->start();
    update();
}


void MainWindow::on_actionPVE_triggered()
{
    delete game;
    game = new GameModel;
    game->changeGameType(PVE);
    labelGameType->setText("当前模式：PVE");
    latestx = latesty = -1;
    stepCount = 0;
    remainingTime = TIME_LIMIT;
    timer->start();
    update();
}


void MainWindow::on_actionSave_triggered()
{
    //存档

    QString dlgTitle = "选择一个文件";
    QString curPath = QCoreApplication::applicationDirPath();
    QString filter = "文本文件(*.txt)";
    QString fileName = QFileDialog::getSaveFileName(this,dlgTitle,curPath,filter);
    if(fileName.isEmpty()) return;
    QFile file(fileName);
    if(file.open(QIODevice::WriteOnly|QIODevice::Text)){
        QString str = "";

        switch(game->getType()){
        case(PVP):
            str += "1";
            break;
        case(PVE):
            str += "2";
            break;
        }

        switch(game->getState()){
        case(Playing):
            str += "1";
            break;
        case(GameOver):
            str += "2";
            break;
        case(Suspension):
            str += "3";
            break;
        }

        switch(game->getPlayer()){
        case(Black):
            str += "1";
            break;
        case(White):
            str += "2";
            break;
        }

        for(int i = 0;i < 11;i++){
            for(int j = 0;j < 11;j++){
                str += game->getBoard(i,j)+'0';
            }
        }
        QByteArray strBytes = str.toUtf8();
        file.write(strBytes,strBytes.length());
        file.close();

        isSaved = true;
    }
}

void MainWindow::on_actionrestore_last_saved_board_triggered()
{
    //读档

    QString dlgTitle = "打开一个文件";
    QString curPath = QCoreApplication::applicationDirPath();
    QString filter = "文本文件(*.txt)";
    QString fileName = QFileDialog::getOpenFileName(this,dlgTitle,curPath,filter);
    if(fileName.isEmpty()) return;
    QFile file(fileName);
    if(file.open(QIODevice::ReadOnly|QIODevice::Text)){
        QString str = file.readAll();
        if(str[0] == '1'){
            on_actionPVP_triggered();
        }else if(str[0] == '2'){
            on_actionPVE_triggered();
        }

        if(str[1] == '1') game->changeGameState(Playing);
        else if(str[1] == '2') game->changeGameState(GameOver);
        else if(str[1] == '3') game->changeGameState(Suspension);
        file.close();

        if(str[2] == '1') game->changePlayer(Black);
        else if(str[2] == '2') game->changePlayer(White);

        int idx = 3;
        for(int i = 0;i < 11;i++){
            for(int j = 0;j < 11;j++){
                if(str[idx] == '0') game->changeBoard(i,j,0);
                if(str[idx] == '1') {
                    game->changeBoard(i,j,1);
                    stepCount++;
                    steps[stepCount][0] = i;
                    steps[stepCount][1] = j;
                    game->updateQi(i,j,1);
                }
                if(str[idx] == '2') {
                    game->changeBoard(i,j,2);
                    stepCount++;
                    steps[stepCount][0] = i;
                    steps[stepCount][1] = j;
                    game->updateQi(i,j,2);
                }
                if(str[idx] == '3') game->changeBoard(i,j,3);
                idx++;
            }
        }
        update();
        file.close();

        isSaved = true;
    }
}

void MainWindow::on_actionShowGameRules_triggered()
{
    //链接到外部网站，获取不围棋规则
    QDesktopServices::openUrl(QUrl("https://baike.baidu.com/item/%E4%B8%8D%E5%9B%B4%E6%A3%8B/19604744?fr=aladdin"));
}

void MainWindow::on_timeOut()
{
    if(game->getState() != Playing){
        timer->stop();
        return;
    }
    remainingTime--;
    labelTimer->setText(QString::asprintf("    剩余时间:%d秒",remainingTime));
    if(remainingTime == 0){
        timer->stop();
        timeLimitExceeded();
    }
}

void MainWindow::timeLimitExceeded()
{
    QString winText[2] = {"白棋超时，黑棋获胜","黑棋超时，白棋获胜"};
    game->changeGameState(GameOver);
    QMessageBox::information(nullptr,"游戏结束",winText[2-game->getPlayer()]);
}


void MainWindow::on_actionRetract_triggered()
{
    if(game->getState() != Playing) return;
    if(stepCount == 0) return;
    if(game->getType() == PVP){
        stepCount--;
        latestx = steps[stepCount][0];
        latesty = steps[stepCount][1];
        delete game;
        game = new GameModel;
        game->changeGameType(PVP);
        if(stepCount % 2 == 0) game->changePlayer(Black);
        else game->changePlayer(White);
        for(int i = 1;i <= stepCount;i++){
            int x = steps[i][0];
            int y = steps[i][1];
            game->changeBoard(x,y,2-i%2);
            game->updateQi(x,y,2-i%2);
        }
        remainingTime = TIME_LIMIT;
        update();
    }
    else if(game->getType() == PVE){
        stepCount -= 2;
        latestx = steps[stepCount][0];
        latesty = steps[stepCount][1];
        delete game;
        game = new GameModel;
        game->changeGameType(PVE);
        game->changePlayer(Black);
        for(int i = 1;i <= stepCount;i++){
            int x = steps[i][0];
            int y = steps[i][1];
            game->changeBoard(x,y,2-i%2);
            game->updateQi(x,y,2-i%2);
        }
        remainingTime = TIME_LIMIT;
        update();
    }
}



void MainWindow::on_actionShowLosingPoints_triggered(bool checked)
{
    Q_UNUSED(checked);
    update();
}


void MainWindow::on_actionShowGameInfo_triggered()
{
    QString str = "欢迎来到NoGo游戏！以下是关于游戏部分操作的说明。\n"
                  "\n"
                  "新游戏：\n"
                  "    PVP模式：黑白棋交替落子，均由玩家通过鼠标点击实现\n"
                  "    PVE模式：玩家执黑，AI执白，玩家鼠标点击落子，AI分析后自动落子\n"
                  "存/读档：\n"
                  "    保存当前状态：在当前目录下选择或创建一个txt文件，将当前棋盘数据保存入该文档中\n"
                  "    恢复上次状态：在当前目录下选择一个txt文件，程序会读入文件中的数据，并显示到棋盘上\n"
                  "技能：\n"
                  "    显示禁手：选中该项后，每次轮到某方落子时，棋盘上会显示出该方不能下的点（用叉表示）\n"
                  "    悔棋：PVP模式下，退回一步；PVE模式下，退回两步，即黑白棋各退回一步\n"
                  "帮助\n"
                  "    不围棋规则：点击后，利用浏览器跳转至百度百科关于不围棋的词条\n"
                  "    操作说明：你可以随时通过点击此项来呼出该窗口\n"
                  "\n"
                  "游戏时间限制："
                  "    每步子的限制时间为30秒，超过30秒自动判负\n"
                  "默认开始游戏："
                  "    进入游戏后，默认为PVE模式，且计时立刻开始\n"
            ;


    QMessageBox msgBox;
    msgBox.setWindowTitle("操作说明");
    msgBox.setInformativeText(str);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.exec();
}

void MainWindow::on_actionPlaceChessMusic_triggered(bool checked)
{
    canPlayMusic = checked;
}
