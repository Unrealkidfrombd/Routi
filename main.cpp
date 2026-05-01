// ============================================================
//  Productivity Suite — Qt6/C++ Cross-Platform Desktop App
//  Build: qt6-base-dev, qt6-charts-dev
//  CMake:  find_package(Qt6 REQUIRED COMPONENTS Widgets Charts Sql)
//          target_link_libraries(app Qt6::Widgets Qt6::Charts Qt6::Sql)
// ============================================================

#include <QStandardPaths>
#include <QApplication>
#include <QMainWindow>
#include <QTabWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QDateEdit>
#include <QTextEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QProgressBar>
#include <QSlider>
#include <QTimer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QSplitter>
#include <QGroupBox>
#include <QScrollArea>
#include <QFrame>
#include <QSpinBox>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QtCharts/QChartView>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>

// ─── DB HELPERS ───────────────────────────────────────────────
static QSqlDatabase db;

void initDB() {
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + "/productivity.db");
    if (!db.open()) { qWarning() << db.lastError(); return; }

    QSqlQuery q;
    q.exec(R"(CREATE TABLE IF NOT EXISTS tasks(
        id INTEGER PRIMARY KEY, text TEXT, category TEXT,
        deadline TEXT, done INTEGER DEFAULT 0, created TEXT))");
    q.exec(R"(CREATE TABLE IF NOT EXISTS habits(
        id INTEGER PRIMARY KEY, name TEXT, category TEXT,
        streak INTEGER DEFAULT 0))");
    q.exec(R"(CREATE TABLE IF NOT EXISTS habit_log(
        habit_id INTEGER, day TEXT))");
    q.exec(R"(CREATE TABLE IF NOT EXISTS notes(
        id INTEGER PRIMARY KEY, title TEXT, body TEXT,
        tag TEXT, created TEXT))");
    q.exec(R"(CREATE TABLE IF NOT EXISTS goals(
        id INTEGER PRIMARY KEY, title TEXT, category TEXT,
        deadline TEXT, progress INTEGER DEFAULT 0))");
    q.exec(R"(CREATE TABLE IF NOT EXISTS milestones(
        id INTEGER PRIMARY KEY, goal_id INTEGER,
        text TEXT, done INTEGER DEFAULT 0))");
    q.exec(R"(CREATE TABLE IF NOT EXISTS pomo(
        id INTEGER PRIMARY KEY CHECK(id=1),
        sessions INTEGER DEFAULT 0))");
    q.exec("INSERT OR IGNORE INTO pomo(id,sessions) VALUES(1,0)");
}

// ─── DASHBOARD ────────────────────────────────────────────────
class DashboardWidget : public QWidget {
    Q_OBJECT
    QLabel *lblTasks, *lblOverdue, *lblHabits, *lblSessions, *lblGoals, *lblNotes;
public:
    DashboardWidget(QWidget *p = nullptr) : QWidget(p) {
        auto *lay = new QVBoxLayout(this);
        lay->setSpacing(16);

        auto *dateLabel = new QLabel(QDate::currentDate().toString("dddd, MMMM d, yyyy"));
        dateLabel->setStyleSheet("color: gray; font-size: 13px;");
        lay->addWidget(dateLabel);

        auto *grid = new QGridLayout();
        grid->setSpacing(10);
        auto addStat = [&](int r, int c, const QString &title, QLabel *&out, const QString &sub) {
            auto *box = new QFrame();
            box->setFrameShape(QFrame::StyledPanel);
            box->setStyleSheet("background: palette(midlight); border-radius: 8px; padding: 10px;");
            auto *vl = new QVBoxLayout(box);
            auto *lbl = new QLabel(title);
            lbl->setStyleSheet("color: gray; font-size: 11px;");
            out = new QLabel("—");
            out->setStyleSheet("font-size: 22px; font-weight: bold;");
            auto *sub2 = new QLabel(sub);
            sub2->setStyleSheet("color: gray; font-size: 11px;");
            vl->addWidget(lbl); vl->addWidget(out); vl->addWidget(sub2);
            grid->addWidget(box, r, c);
        };
        addStat(0,0,"Tasks done", lblTasks, "completed");
        addStat(0,1,"Overdue",    lblOverdue, "tasks");
        addStat(0,2,"Habits today", lblHabits, "checked");
        addStat(1,0,"Focus sessions", lblSessions, "pomodoros");
        addStat(1,1,"Goal progress", lblGoals, "avg %");
        addStat(1,2,"Notes", lblNotes, "saved");
        lay->addLayout(grid);
        lay->addStretch();
        refresh();
    }

    void refresh() {
        QSqlQuery q;
        q.exec("SELECT COUNT(*) FROM tasks WHERE done=1"); q.next();
        lblTasks->setText(q.value(0).toString());
        q.exec(QString("SELECT COUNT(*) FROM tasks WHERE done=0 AND deadline<'%1'")
               .arg(QDate::currentDate().toString("yyyy-MM-dd")));
        q.next(); lblOverdue->setText(q.value(0).toString());
        q.exec(QString("SELECT COUNT(*) FROM habit_log WHERE day='%1'")
               .arg(QDate::currentDate().toString("yyyy-MM-dd")));
        q.next(); lblHabits->setText(q.value(0).toString());
        q.exec("SELECT sessions FROM pomo WHERE id=1"); q.next();
        lblSessions->setText(q.value(0).toString());
        q.exec("SELECT AVG(progress) FROM goals"); q.next();
        lblGoals->setText(q.value(0).toString().left(4) + "%");
        q.exec("SELECT COUNT(*) FROM notes"); q.next();
        lblNotes->setText(q.value(0).toString());
    }
};

// ─── TASKS ────────────────────────────────────────────────────
class TasksWidget : public QWidget {
    Q_OBJECT
    QListWidget *list;
    QLineEdit *txtInput;
    QComboBox *cbCat;
    QDateEdit *dtDeadline;
public:
    TasksWidget(QWidget *p = nullptr) : QWidget(p) {
        auto *lay = new QVBoxLayout(this);
        auto *row = new QHBoxLayout();
        txtInput = new QLineEdit(); txtInput->setPlaceholderText("New task...");
        cbCat = new QComboBox();
        for (auto &c : {"Work","Personal","Health","Learning","Other"}) cbCat->addItem(c);
        dtDeadline = new QDateEdit(QDate::currentDate()); dtDeadline->setCalendarPopup(true);
        auto *btn = new QPushButton("Add");
        row->addWidget(txtInput,3); row->addWidget(cbCat,1); row->addWidget(dtDeadline,2); row->addWidget(btn);
        lay->addLayout(row);
        list = new QListWidget(); lay->addWidget(list);
        connect(btn, &QPushButton::clicked, this, &TasksWidget::addTask);
        connect(txtInput, &QLineEdit::returnPressed, this, &TasksWidget::addTask);
        loadTasks();
    }

    void loadTasks() {
        list->clear();
        QSqlQuery q("SELECT id,text,category,deadline,done FROM tasks ORDER BY done,created DESC");
        while (q.next()) {
            auto *item = new QListWidgetItem();
            bool done = q.value(4).toBool();
            QString text = QString("[%1] %2  —  %3  —  %4")
                .arg(q.value(2).toString(), q.value(1).toString(),
                     q.value(3).toString(), done ? "✓ done" : "pending");
            item->setText(text);
            item->setData(Qt::UserRole, q.value(0));
            item->setCheckState(done ? Qt::Checked : Qt::Unchecked);
            if (done) item->setForeground(Qt::gray);
            list->addItem(item);
        }
    }

    void addTask() {
        if (txtInput->text().trimmed().isEmpty()) return;
        QSqlQuery q;
        q.prepare("INSERT INTO tasks(text,category,deadline,done,created) VALUES(?,?,?,0,?)");
        q.addBindValue(txtInput->text().trimmed());
        q.addBindValue(cbCat->currentText());
        q.addBindValue(dtDeadline->date().toString("yyyy-MM-dd"));
        q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
        q.exec();
        txtInput->clear();
        loadTasks();
        emit dataChanged();
    }

protected:
    void mousePressEvent(QMouseEvent *e) override {
        auto *item = list->itemAt(list->mapFromParent(e->pos()));
        if (!item) { QWidget::mousePressEvent(e); return; }
        int id = item->data(Qt::UserRole).toInt();
        bool nowDone = item->checkState() == Qt::Checked;
        QSqlQuery q;
        q.prepare("UPDATE tasks SET done=? WHERE id=?");
        q.addBindValue(nowDone ? 1 : 0); q.addBindValue(id); q.exec();
        loadTasks(); emit dataChanged();
        QWidget::mousePressEvent(e);
    }

signals:
    void dataChanged();
};

// ─── HABITS ───────────────────────────────────────────────────
class HabitsWidget : public QWidget {
    Q_OBJECT
    QListWidget *list;
    QLineEdit *nameInput;
    QComboBox *cbCat;
public:
    HabitsWidget(QWidget *p = nullptr) : QWidget(p) {
        auto *lay = new QVBoxLayout(this);
        auto *row = new QHBoxLayout();
        nameInput = new QLineEdit(); nameInput->setPlaceholderText("New habit...");
        cbCat = new QComboBox();
        for (auto &c : {"Health","Work","Personal","Learning","Other"}) cbCat->addItem(c);
        auto *btn = new QPushButton("Add");
        row->addWidget(nameInput,3); row->addWidget(cbCat,1); row->addWidget(btn);
        lay->addLayout(row);
        list = new QListWidget(); lay->addWidget(list);
        connect(btn, &QPushButton::clicked, this, &HabitsWidget::addHabit);
        connect(nameInput, &QLineEdit::returnPressed, this, &HabitsWidget::addHabit);
        connect(list, &QListWidget::itemChanged, this, &HabitsWidget::onCheck);
        loadHabits();
    }

    void loadHabits() {
        list->blockSignals(true); list->clear();
        QString today = QDate::currentDate().toString("yyyy-MM-dd");
        QSqlQuery q("SELECT h.id,h.name,h.category,h.streak,"
                    "(SELECT COUNT(*) FROM habit_log WHERE habit_id=h.id AND day='" + today + "') as done "
                    "FROM habits h");
        while (q.next()) {
            auto *item = new QListWidgetItem(
                QString("%1  [%2]  — streak: %3 days")
                .arg(q.value(1).toString(), q.value(2).toString(), q.value(3).toString()));
            item->setData(Qt::UserRole, q.value(0));
            item->setCheckState(q.value(4).toInt() ? Qt::Checked : Qt::Unchecked);
            list->addItem(item);
        }
        list->blockSignals(false);
    }

    void addHabit() {
        if (nameInput->text().trimmed().isEmpty()) return;
        QSqlQuery q;
        q.prepare("INSERT INTO habits(name,category) VALUES(?,?)");
        q.addBindValue(nameInput->text().trimmed());
        q.addBindValue(cbCat->currentText());
        q.exec(); nameInput->clear(); loadHabits(); emit dataChanged();
    }

    void onCheck(QListWidgetItem *item) {
        int id = item->data(Qt::UserRole).toInt();
        QString today = QDate::currentDate().toString("yyyy-MM-dd");
        QSqlQuery q;
        if (item->checkState() == Qt::Checked) {
            q.prepare("INSERT OR IGNORE INTO habit_log(habit_id,day) VALUES(?,?)");
            q.addBindValue(id); q.addBindValue(today); q.exec();
        } else {
            q.prepare("DELETE FROM habit_log WHERE habit_id=? AND day=?");
            q.addBindValue(id); q.addBindValue(today); q.exec();
        }
        // recalc streak
        int streak = 0;
        QDate d = QDate::currentDate();
        while (true) {
            q.prepare("SELECT COUNT(*) FROM habit_log WHERE habit_id=? AND day=?");
            q.addBindValue(id); q.addBindValue(d.toString("yyyy-MM-dd")); q.exec(); q.next();
            if (!q.value(0).toInt()) break;
            streak++; d = d.addDays(-1);
        }
        q.prepare("UPDATE habits SET streak=? WHERE id=?");
        q.addBindValue(streak); q.addBindValue(id); q.exec();
        loadHabits(); emit dataChanged();
    }

signals:
    void dataChanged();
};

// ─── POMODORO ─────────────────────────────────────────────────
class PomodoroWidget : public QWidget {
    Q_OBJECT
    QTimer *timer;
    QLabel *timeLbl, *modeLbl, *sessLbl;
    QPushButton *startBtn, *resetBtn;
    QProgressBar *prog;
    int remaining, total, sessions = 0;
    bool isWork = true;
public:
    PomodoroWidget(QWidget *p = nullptr) : QWidget(p) {
        QSqlQuery q("SELECT sessions FROM pomo WHERE id=1");
        if (q.next()) sessions = q.value(0).toInt();

        auto *lay = new QVBoxLayout(this);
        lay->setAlignment(Qt::AlignCenter);

        modeLbl = new QLabel("Focus");
        modeLbl->setAlignment(Qt::AlignCenter);
        modeLbl->setStyleSheet("font-size:18px; font-weight:500;");

        timeLbl = new QLabel("25:00");
        timeLbl->setAlignment(Qt::AlignCenter);
        timeLbl->setStyleSheet("font-size:72px; font-weight:300; font-family:monospace;");

        prog = new QProgressBar(); prog->setRange(0,100); prog->setValue(0);
        prog->setFixedHeight(8); prog->setTextVisible(false);

        sessLbl = new QLabel(QString("%1 sessions completed").arg(sessions));
        sessLbl->setAlignment(Qt::AlignCenter);
        sessLbl->setStyleSheet("color:gray; font-size:14px;");

        auto *btnRow = new QHBoxLayout();
        startBtn = new QPushButton("Start"); startBtn->setFixedWidth(120);
        resetBtn = new QPushButton("Reset"); resetBtn->setFixedWidth(80);
        btnRow->addStretch(); btnRow->addWidget(startBtn); btnRow->addWidget(resetBtn); btnRow->addStretch();

        lay->addStretch();
        lay->addWidget(modeLbl); lay->addWidget(timeLbl); lay->addWidget(prog);
        lay->addLayout(btnRow); lay->addWidget(sessLbl);
        lay->addStretch();

        timer = new QTimer(this);
        setWork();
        connect(startBtn, &QPushButton::clicked, this, &PomodoroWidget::toggleTimer);
        connect(resetBtn, &QPushButton::clicked, this, &PomodoroWidget::reset);
        connect(timer, &QTimer::timeout, this, &PomodoroWidget::tick);
    }

    void setWork() { isWork=true; total=remaining=25*60; modeLbl->setText("Focus"); updateDisplay(); }
    void setBreak(){ isWork=false;total=remaining=5*60; modeLbl->setText("Break"); updateDisplay(); }

    void updateDisplay() {
        timeLbl->setText(QString("%1:%2")
            .arg(remaining/60, 2, 10, QChar('0'))
            .arg(remaining%60, 2, 10, QChar('0')));
        prog->setValue(100 - int(100.0*remaining/total));
    }

    void toggleTimer() {
        if (timer->isActive()) { timer->stop(); startBtn->setText("Resume"); }
        else { timer->start(1000); startBtn->setText("Pause"); }
    }

    void reset() {
        timer->stop(); startBtn->setText("Start");
        if (isWork) setWork(); else setBreak();
    }

    void tick() {
        if (--remaining <= 0) {
            timer->stop(); startBtn->setText("Start");
            if (isWork) {
                sessions++;
                QSqlQuery q;
                q.prepare("UPDATE pomo SET sessions=? WHERE id=1");
                q.addBindValue(sessions); q.exec();
                sessLbl->setText(QString("%1 sessions completed").arg(sessions));
                emit dataChanged();
                QMessageBox::information(this, "Pomodoro", "Focus session done! Time for a break.");
                setBreak();
            } else {
                QMessageBox::information(this, "Pomodoro", "Break over — back to focus!");
                setWork();
            }
        } else updateDisplay();
    }

signals:
    void dataChanged();
};

// ─── NOTES ────────────────────────────────────────────────────
class NotesWidget : public QWidget {
    Q_OBJECT
    QListWidget *noteList;
    QLineEdit *titleEdit, *searchEdit;
    QTextEdit *bodyEdit;
    QComboBox *tagBox;
    QPushButton *saveBtn, *newBtn, *delBtn;
    int currentId = -1;
public:
    NotesWidget(QWidget *p = nullptr) : QWidget(p) {
        auto *lay = new QHBoxLayout(this);
        auto *left = new QVBoxLayout();
        searchEdit = new QLineEdit(); searchEdit->setPlaceholderText("Search...");
        newBtn = new QPushButton("+ New");
        noteList = new QListWidget(); noteList->setFixedWidth(200);
        left->addWidget(searchEdit); left->addWidget(newBtn); left->addWidget(noteList);

        auto *right = new QVBoxLayout();
        auto *row = new QHBoxLayout();
        titleEdit = new QLineEdit(); titleEdit->setPlaceholderText("Title");
        tagBox = new QComboBox();
        for (auto &c : {"Work","Personal","Health","Learning","Other"}) tagBox->addItem(c);
        row->addWidget(titleEdit,3); row->addWidget(tagBox,1);
        bodyEdit = new QTextEdit(); bodyEdit->setPlaceholderText("Write your note...");
        auto *btnRow = new QHBoxLayout();
        saveBtn = new QPushButton("Save"); delBtn = new QPushButton("Delete");
        btnRow->addWidget(saveBtn); btnRow->addStretch(); btnRow->addWidget(delBtn);
        right->addLayout(row); right->addWidget(bodyEdit); right->addLayout(btnRow);

        lay->addLayout(left); lay->addLayout(right,1);
        connect(newBtn, &QPushButton::clicked, this, &NotesWidget::newNote);
        connect(saveBtn, &QPushButton::clicked, this, &NotesWidget::saveNote);
        connect(delBtn, &QPushButton::clicked, this, &NotesWidget::delNote);
        connect(noteList, &QListWidget::currentItemChanged, this, &NotesWidget::selectNote);
        connect(searchEdit, &QLineEdit::textChanged, this, &NotesWidget::loadNotes);
        loadNotes();
    }

    void loadNotes() {
        noteList->clear();
        QString s = searchEdit->text();
        QSqlQuery q;
        q.prepare("SELECT id,title,tag FROM notes WHERE title LIKE ? OR body LIKE ? ORDER BY created DESC");
        q.addBindValue("%" + s + "%"); q.addBindValue("%" + s + "%"); q.exec();
        while (q.next()) {
            auto *item = new QListWidgetItem(q.value(1).toString() + " [" + q.value(2).toString() + "]");
            item->setData(Qt::UserRole, q.value(0)); noteList->addItem(item);
        }
    }

    void newNote() { currentId=-1; titleEdit->clear(); bodyEdit->clear(); tagBox->setCurrentIndex(0); }

    void saveNote() {
        if (titleEdit->text().trimmed().isEmpty()) return;
        QSqlQuery q;
        if (currentId == -1) {
            q.prepare("INSERT INTO notes(title,body,tag,created) VALUES(?,?,?,?)");
            q.addBindValue(titleEdit->text().trimmed());
            q.addBindValue(bodyEdit->toPlainText());
            q.addBindValue(tagBox->currentText());
            q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
        } else {
            q.prepare("UPDATE notes SET title=?,body=?,tag=? WHERE id=?");
            q.addBindValue(titleEdit->text().trimmed());
            q.addBindValue(bodyEdit->toPlainText());
            q.addBindValue(tagBox->currentText());
            q.addBindValue(currentId);
        }
        q.exec(); loadNotes(); emit dataChanged();
    }

    void delNote() {
        if (currentId == -1) return;
        QSqlQuery q; q.prepare("DELETE FROM notes WHERE id=?"); q.addBindValue(currentId); q.exec();
        currentId = -1; newNote(); loadNotes(); emit dataChanged();
    }

    void selectNote(QListWidgetItem *item) {
        if (!item) return;
        currentId = item->data(Qt::UserRole).toInt();
        QSqlQuery q; q.prepare("SELECT title,body,tag FROM notes WHERE id=?");
        q.addBindValue(currentId); q.exec();
        if (q.next()) {
            titleEdit->setText(q.value(0).toString());
            bodyEdit->setPlainText(q.value(1).toString());
            tagBox->setCurrentText(q.value(2).toString());
        }
    }

signals:
    void dataChanged();
};

// ─── GOALS ────────────────────────────────────────────────────
class GoalsWidget : public QWidget {
    Q_OBJECT
    QListWidget *goalList;
    QLineEdit *titleInput, *msInput;
    QComboBox *cbCat;
    QDateEdit *dtDeadline;
    QSlider *progSlider;
    QLabel *progLbl;
    QListWidget *msList;
    int currentGoalId = -1;
public:
    GoalsWidget(QWidget *p = nullptr) : QWidget(p) {
        auto *lay = new QVBoxLayout(this);
        // Add goal row
        auto *row = new QHBoxLayout();
        titleInput = new QLineEdit(); titleInput->setPlaceholderText("Goal title...");
        cbCat = new QComboBox();
        for (auto &c : {"Work","Personal","Health","Learning","Other"}) cbCat->addItem(c);
        dtDeadline = new QDateEdit(QDate::currentDate()); dtDeadline->setCalendarPopup(true);
        auto *addBtn = new QPushButton("Add goal");
        row->addWidget(titleInput,3); row->addWidget(cbCat,1); row->addWidget(dtDeadline,2); row->addWidget(addBtn);
        lay->addLayout(row);

        auto *split = new QSplitter(Qt::Horizontal);
        goalList = new QListWidget();
        auto *detailW = new QWidget();
        auto *dlay = new QVBoxLayout(detailW);
        progSlider = new QSlider(Qt::Horizontal); progSlider->setRange(0,100);
        progLbl = new QLabel("0%"); progLbl->setStyleSheet("font-size:16px; font-weight:bold;");
        auto *prow = new QHBoxLayout(); prow->addWidget(new QLabel("Progress:")); prow->addWidget(progSlider,1); prow->addWidget(progLbl);
        auto *msRow = new QHBoxLayout();
        msInput = new QLineEdit(); msInput->setPlaceholderText("Add milestone...");
        auto *msBtn = new QPushButton("Add");
        msRow->addWidget(msInput,3); msRow->addWidget(msBtn);
        msList = new QListWidget();
        dlay->addLayout(prow); dlay->addWidget(new QLabel("Milestones:")); dlay->addLayout(msRow); dlay->addWidget(msList);

        split->addWidget(goalList); split->addWidget(detailW);
        split->setSizes({200, 400});
        lay->addWidget(split);

        connect(addBtn, &QPushButton::clicked, this, &GoalsWidget::addGoal);
        connect(titleInput, &QLineEdit::returnPressed, this, &GoalsWidget::addGoal);
        connect(goalList, &QListWidget::currentItemChanged, this, &GoalsWidget::selectGoal);
        connect(progSlider, &QSlider::valueChanged, this, &GoalsWidget::updateProgress);
        connect(msBtn, &QPushButton::clicked, this, &GoalsWidget::addMilestone);
        connect(msInput, &QLineEdit::returnPressed, this, &GoalsWidget::addMilestone);
        connect(msList, &QListWidget::itemChanged, this, &GoalsWidget::toggleMilestone);
        loadGoals();
    }

    void loadGoals() {
        goalList->clear();
        QSqlQuery q("SELECT id,title,category,progress,deadline FROM goals");
        while (q.next()) {
            auto *item = new QListWidgetItem(
                QString("%1  [%2]  %3%").arg(q.value(1).toString(), q.value(2).toString(), q.value(3).toString()));
            item->setData(Qt::UserRole, q.value(0)); goalList->addItem(item);
        }
    }

    void addGoal() {
        if (titleInput->text().trimmed().isEmpty()) return;
        QSqlQuery q;
        q.prepare("INSERT INTO goals(title,category,deadline,progress) VALUES(?,?,?,0)");
        q.addBindValue(titleInput->text().trimmed());
        q.addBindValue(cbCat->currentText());
        q.addBindValue(dtDeadline->date().toString("yyyy-MM-dd"));
        q.exec(); titleInput->clear(); loadGoals(); emit dataChanged();
    }

    void selectGoal(QListWidgetItem *item) {
        if (!item) return;
        currentGoalId = item->data(Qt::UserRole).toInt();
        QSqlQuery q; q.prepare("SELECT progress FROM goals WHERE id=?");
        q.addBindValue(currentGoalId); q.exec();
        if (q.next()) { progSlider->blockSignals(true); progSlider->setValue(q.value(0).toInt()); progSlider->blockSignals(false); progLbl->setText(q.value(0).toString() + "%"); }
        loadMilestones();
    }

    void updateProgress(int v) {
        if (currentGoalId == -1) return;
        progLbl->setText(QString::number(v) + "%");
        QSqlQuery q; q.prepare("UPDATE goals SET progress=? WHERE id=?");
        q.addBindValue(v); q.addBindValue(currentGoalId); q.exec();
        loadGoals(); emit dataChanged();
    }

    void loadMilestones() {
        msList->blockSignals(true); msList->clear();
        QSqlQuery q; q.prepare("SELECT id,text,done FROM milestones WHERE goal_id=?");
        q.addBindValue(currentGoalId); q.exec();
        while (q.next()) {
            auto *item = new QListWidgetItem(q.value(1).toString());
            item->setData(Qt::UserRole, q.value(0));
            item->setCheckState(q.value(2).toInt() ? Qt::Checked : Qt::Unchecked);
            msList->addItem(item);
        }
        msList->blockSignals(false);
    }

    void addMilestone() {
        if (currentGoalId == -1 || msInput->text().trimmed().isEmpty()) return;
        QSqlQuery q; q.prepare("INSERT INTO milestones(goal_id,text,done) VALUES(?,?,0)");
        q.addBindValue(currentGoalId); q.addBindValue(msInput->text().trimmed()); q.exec();
        msInput->clear(); loadMilestones();
    }

    void toggleMilestone(QListWidgetItem *item) {
        QSqlQuery q; q.prepare("UPDATE milestones SET done=? WHERE id=?");
        q.addBindValue(item->checkState() == Qt::Checked ? 1 : 0);
        q.addBindValue(item->data(Qt::UserRole)); q.exec();
    }

signals:
    void dataChanged();
};

// ─── MAIN WINDOW ──────────────────────────────────────────────
class MainWindow : public QMainWindow {
    Q_OBJECT
    DashboardWidget *dash;
    QTabWidget *tabs;
public:
    MainWindow(QWidget *p = nullptr) : QMainWindow(p) {
        setWindowTitle("Productivity Suite");
        resize(900, 620);

        tabs = new QTabWidget(this);
        setCentralWidget(tabs);

        dash = new DashboardWidget();
        auto *tasksW   = new TasksWidget();
        auto *habitsW  = new HabitsWidget();
        auto *pomoW    = new PomodoroWidget();
        auto *notesW   = new NotesWidget();
        auto *goalsW   = new GoalsWidget();

        tabs->addTab(dash,    "Dashboard");
        tabs->addTab(tasksW,  "Tasks");
        tabs->addTab(habitsW, "Habits");
        tabs->addTab(pomoW,   "Focus");
        tabs->addTab(notesW,  "Notes");
        tabs->addTab(goalsW,  "Goals");

        // refresh dashboard on data changes
        auto refresh = [this]{ dash->refresh(); };
        connect(tasksW,  &TasksWidget::dataChanged,   this, refresh);
        connect(habitsW, &HabitsWidget::dataChanged,  this, refresh);
        connect(pomoW,   &PomodoroWidget::dataChanged,this, refresh);
        connect(notesW,  &NotesWidget::dataChanged,   this, refresh);
        connect(goalsW,  &GoalsWidget::dataChanged,   this, refresh);
    }
};

// ─── ENTRY POINT ──────────────────────────────────────────────
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("ProductivitySuite");
    QApplication::setOrganizationName("YourOrg");

    initDB();

    MainWindow w;
    w.show();
    return app.exec();
}

#include "main.moc"
