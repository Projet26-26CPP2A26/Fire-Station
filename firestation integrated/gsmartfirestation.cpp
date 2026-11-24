#include "gsmartfirestation.h"
#include "ui_gsmartfirestation.h"

// ===== databasemanager =====
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QSqlError>

QSqlDatabase DatabaseManager::s_db;
bool DatabaseManager::s_connected = false;

bool DatabaseManager::connectInteractive(QWidget *parent)
{
    // Connexion via ODBC (pilote Windows "Oracle in XE", 64 bits).
    if (QSqlDatabase::isDriverAvailable("QODBC") == false) {
        QMessageBox::warning(parent, "Pilote ODBC manquant",
            "Le pilote Qt QODBC n'est pas disponible.\n"
            "L'application peut s'ouvrir en mode démonstration, mais ne pourra pas lire/écrire dans Oracle.");
        return false;
    }

    bool ok = false;
    QString host = QInputDialog::getText(parent, "Connexion Oracle",
        "Hôte Oracle :", QLineEdit::Normal, "localhost", &ok);
    if (!ok) return false;
    if (host.trimmed().isEmpty()) host = "localhost";

    int port = QInputDialog::getInt(parent, "Connexion Oracle",
        "Port Oracle :", 1521, 1, 65535, 1, &ok);
    if (!ok) return false;

    QString sid = QInputDialog::getText(parent, "Connexion Oracle",
        "SID ou nom de service (ex. XE) :", QLineEdit::Normal, "XE", &ok);
    if (!ok || sid.trimmed().isEmpty()) return false;

    QString user = QInputDialog::getText(parent, "Connexion Oracle",
        "Nom d'utilisateur Oracle :", QLineEdit::Normal, "", &ok);
    if (!ok || user.trimmed().isEmpty()) return false;

    QString password = QInputDialog::getText(parent, "Connexion Oracle",
        "Mot de passe Oracle :", QLineEdit::Password, "", &ok);
    if (!ok) return false;

    const QString connectionName = "SmartFireStationOracle";
    if (QSqlDatabase::contains(connectionName))
        QSqlDatabase::removeDatabase(connectionName);

    s_db = QSqlDatabase::addDatabase("QODBC", connectionName);
    // Qt ajoute lui-même UID= et PWD= à partir de setUserName / setPassword.
    s_db.setDatabaseName(QString("DRIVER={Oracle in XE};DBQ=//%1:%2/%3;")
                             .arg(host.trimmed())
                             .arg(port)
                             .arg(sid.trimmed()));
    s_db.setUserName(user.trimmed());
    s_db.setPassword(password);

    if (!s_db.open()) {
        const QString error = s_db.lastError().text();
        s_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
        s_connected = false;
        QMessageBox::warning(parent, "Connexion Oracle échouée",
            "Impossible de se connecter à Oracle.\n\nDétail : " + error +
            "\n\nVérifiez l'hôte, le port, le SID/service, l'utilisateur, le mot de passe "
            "et que le service Oracle est démarré.");
        return false;
    }

    s_connected = true;
    QMessageBox::information(parent, "Oracle connecté",
        "Connexion Oracle établie.\nLes données seront chargées depuis la base.");
    return true;
}

bool DatabaseManager::isConnected() { return s_connected && s_db.isOpen(); }
QSqlDatabase DatabaseManager::database() { return s_db; }
void DatabaseManager::close()
{
    if (s_db.isValid()) s_db.close();
    s_connected = false;
}

// ===== pdfexport =====

#include <QDateTime>
#include <QFileInfo>
#include <QPdfWriter>
#include <QTextDocument>

bool creerPdfTableau(const QString &chemin, const QString &titre,
                     const QStringList &entetes, const QList<QStringList> &lignes)
{
    // 1) On écrit le contenu du PDF en HTML (titre + tableau)
    QString html = "<h2 style='color:#1F4E79;'>Smart Fire Station Management</h2>";
    html += "<h3>" + titre + "</h3>";
    html += "<p>Exporté le " + QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm") + "</p>";
    html += "<table border='1' cellspacing='0' cellpadding='5' width='100%'><tr>";
    for (int c = 0; c < entetes.size(); c++)
        html += "<th bgcolor='#1F4E79' style='color:white;'>" + entetes[c] + "</th>";
    html += "</tr>";
    for (int l = 0; l < lignes.size(); l++) {
        html += "<tr>";
        for (int c = 0; c < lignes[l].size(); c++)
            html += "<td>" + lignes[l][c] + "</td>";
        html += "</tr>";
    }
    html += "</table>";

    // 2) On "imprime" ce HTML dans un fichier PDF
    QPdfWriter writer(chemin);
    QTextDocument document;
    document.setHtml(html);
    document.print(&writer);

    return QFileInfo(chemin).size() > 0;
}

// ===== vehicule =====

Vehicule::Vehicule()
{
}

Vehicule::Vehicule(const QString &id, const QString &immatriculation, const QString &type,
                   const QString &modele, const QString &etat, const QString &disponibilite,
                   const QDate &dateMaintenance)
{
    m_id = id;
    m_immatriculation = immatriculation;
    m_type = type;
    m_modele = modele;
    m_etat = etat;
    m_disponibilite = disponibilite;
    m_dateMaintenance = dateMaintenance;
}

bool Vehicule::maintenanceProche() const
{
    QDate limite = QDate::currentDate().addDays(30);
    return m_dateMaintenance.isValid() && m_dateMaintenance <= limite;
}

QStringList Vehicule::types()
{
    QStringList liste;
    liste << "Fourgon" << "Camion" << "Ambulance" << "Citerne" << "Échelle";
    return liste;
}

QStringList Vehicule::etats()
{
    QStringList liste;
    liste << "Bon" << "À contrôler" << "Hors service";
    return liste;
}

QStringList Vehicule::disponibilites()
{
    QStringList liste;
    liste << "Disponible" << "En mission" << "En maintenance";
    return liste;
}

// Types de véhicules adaptés à chaque intervention, par ordre de préférence
QStringList Vehicule::typesPourIntervention(const QString &intervention)
{
    QStringList liste;
    if (intervention == "Incendie")
        liste << "Citerne" << "Fourgon" << "Échelle";
    else if (intervention == "Secours à personne")
        liste << "Ambulance" << "Fourgon";
    else if (intervention == "Accident de la route")
        liste << "Camion" << "Fourgon" << "Ambulance";
    else if (intervention == "Inondation")
        liste << "Camion" << "Citerne";
    else
        liste = types();
    return liste;
}

// ===== piechartwidget =====
#include <QPainter>
#include <QPaintEvent>
#include <QFont>
#include <QRectF>
#include <QtMath>

PieChartWidget::PieChartWidget(const QString &centerLabel, QWidget *parent)
    : QWidget(parent), m_centerLabel(centerLabel)
{
    setMinimumHeight(235);
    setMinimumWidth(250);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}
void PieChartWidget::setData(const QStringList &labels, const QList<int> &values)
{
    m_labels = labels; m_values = values; update();
}
void PieChartWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor colors[] = {QColor("#1F4E79"), QColor("#DC3C2D"), QColor("#2E7D32"), QColor("#E6A23C"), QColor("#7C3AED"), QColor("#0891B2"), QColor("#64748B"), QColor("#DB2777")};
    int total = 0;
    for (int v : m_values) total += qMax(0, v);
    // Responsive layout: reserve a clear legend area and keep the circle fully visible.
    const int legendWidth = qMin(190, qMax(112, width() * 46 / 100));
    const int availablePie = qMax(90, width() - legendWidth - 24);
    const int diameter = qMax(90, qMin(170, qMin(availablePie - 8, height() - 24)));
    const int px = 8;
    const int py = qMax(8, (height() - diameter) / 2);
    QRectF pie(px, py, diameter, diameter);
    if (total <= 0) {
        p.setPen(QPen(QColor("#E5E7EB"), 18)); p.setBrush(Qt::NoBrush);
        p.drawEllipse(pie.adjusted(10,10,-10,-10));
        p.setPen(QColor("#64748B")); p.drawText(pie, Qt::AlignCenter, "Aucune donnée"); return;
    }
    int start = 90 * 16;
    for (int i=0; i<m_values.size(); ++i) {
        int val = qMax(0,m_values[i]); if (!val) continue;
        int span = -qRound(360.0 * 16 * val / total);
        p.setPen(QPen(Qt::white, 1.5)); p.setBrush(colors[i % 8]); p.drawPie(pie, start, span); start += span;
    }
    int hole = diameter / 3;
    p.setPen(Qt::white); p.setBrush(Qt::white); p.drawEllipse(pie.adjusted(hole,hole,-hole,-hole));
    p.setPen(QColor("#1F2937")); QFont f=p.font(); f.setBold(true); f.setPointSize(17); p.setFont(f);
    p.drawText(QRectF(pie.x()+diameter/4, pie.y()+diameter/2-18, diameter/2, 24), Qt::AlignCenter, QString::number(total));
    f.setBold(false); f.setPointSize(8); p.setFont(f); p.setPen(QColor("#64748B"));
    p.drawText(QRectF(pie.x()+diameter/4, pie.y()+diameter/2+5, diameter/2, 18), Qt::AlignCenter, QStringLiteral("Total"));
    const int legendX = px + diameter + 12;
    const int lineHeight = qMin(28, qMax(20, (height()-16) / qMax(1,m_labels.size())));
    f=p.font(); f.setPointSize(8); p.setFont(f);
    for (int i=0; i<m_labels.size() && i<m_values.size(); ++i) {
        int y = 8 + i * lineHeight;
        p.setPen(Qt::NoPen); p.setBrush(colors[i%8]); p.drawRoundedRect(QRectF(legendX,y+5,9,9),2,2);
        double pct = total ? (100.0*m_values[i]/total) : 0;
        p.setPen(QColor("#334155"));
        QString line = QString("%1  %2 (%3%)").arg(m_labels[i]).arg(m_values[i]).arg(qRound(pct));
        QRectF textRect(legendX+14,y,width()-legendX-17,lineHeight);
        p.drawText(textRect, Qt::AlignLeft|Qt::AlignVCenter|Qt::TextWordWrap, line);
    }
}

// ===== statsdialog =====

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace {
class DonutChart : public QWidget
{
public:
    DonutChart(const QStringList &labels, const QList<int> &values, QWidget *parent = 0)
        : QWidget(parent), m_labels(labels), m_values(values)
    {
        setMinimumSize(280, 230);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFixedHeight(250);
    }
protected:
    void paintEvent(QPaintEvent *)
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const int total = [&]() { int t=0; for (int v : m_values) t += v; return t; }();
        QRectF rect((width()-190)/2.0, 14, 190, 190);
        const QColor colors[] = {QColor("#2E7D32"), QColor("#DC3C2D"), QColor("#1F4E79"), QColor("#E6A23C"), QColor("#7E57C2"), QColor("#607D8B")};
        if (total <= 0) {
            painter.setPen(QPen(QColor("#E5E7EB"), 26)); painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(rect.adjusted(18,18,-18,-18));
            painter.setPen(QColor("#64748B")); painter.drawText(rect, Qt::AlignCenter, "Aucune donnée");
            return;
        }
        int start = 90 * 16;
        for (int i=0; i<m_values.size(); ++i) {
            int span = -qRound(360.0 * 16 * m_values[i] / total);
            painter.setPen(QPen(Qt::white, 2)); painter.setBrush(colors[i % 6]);
            painter.drawPie(rect, start, span); start += span;
        }
        painter.setPen(Qt::white); painter.setBrush(Qt::white);
        painter.drawEllipse(rect.adjusted(47,47,-47,-47));
        painter.setPen(QColor("#1F2937"));
        QFont f = painter.font(); f.setBold(true); f.setPointSize(20); painter.setFont(f);
        painter.drawText(rect.adjusted(35,55,-35,-55), Qt::AlignCenter, QString::number(total));
        f.setBold(false); f.setPointSize(9); painter.setFont(f); painter.setPen(QColor("#64748B"));
        painter.drawText(rect.adjusted(35,87,-35,-30), Qt::AlignCenter, "TOTAL");
    }
private:
    QStringList m_labels;
    QList<int> m_values;
};
}

StatCritere compterParValeur(const QString &nom, const QStringList &possibles,
                             const QStringList &reelles)
{
    StatCritere resultat; resultat.nom = nom;
    for (int p=0; p<possibles.size(); ++p) {
        int n=0; for (int r=0; r<reelles.size(); ++r) if (reelles[r] == possibles[p]) ++n;
        if (n>0) { resultat.etiquettes << possibles[p]; resultat.valeurs << n; }
    }
    return resultat;
}

StatsDialog::StatsDialog(const QString &titre, const QList<StatCritere> &criteres, QWidget *parent)
    : QDialog(parent)
{
    m_criteres=criteres; setWindowTitle(titre); setMinimumSize(540, 480);
    QLabel *labelTitre=new QLabel(titre); labelTitre->setProperty("class","cardTitle");
    m_combo=new QComboBox;
    for (int i=0;i<m_criteres.size();++i) m_combo->addItem(m_criteres[i].nom);
    QHBoxLayout *ligneCritere=new QHBoxLayout;
    ligneCritere->addWidget(new QLabel("Répartition selon :")); ligneCritere->addWidget(m_combo); ligneCritere->addStretch();
    m_grille=new QGridLayout; m_grille->setVerticalSpacing(10);
    QPushButton *fermer=new QPushButton("Fermer"); fermer->setProperty("class","btnPrimary");
    connect(fermer,SIGNAL(clicked()),this,SLOT(close()));
    QHBoxLayout *ligneBoutons=new QHBoxLayout; ligneBoutons->addStretch(); ligneBoutons->addWidget(fermer);
    QVBoxLayout *principal=new QVBoxLayout(this); principal->setContentsMargins(24,20,24,20); principal->setSpacing(12);
    principal->addWidget(labelTitre); principal->addLayout(ligneCritere); principal->addLayout(m_grille,1); principal->addLayout(ligneBoutons);
    connect(m_combo,SIGNAL(currentIndexChanged(int)),this,SLOT(afficherCritere(int)));
    afficherCritere(0); repolish(this);
}

void StatsDialog::afficherCritere(int index)
{
    if (index<0 || index>=m_criteres.size()) return;
    while (QLayoutItem *item=m_grille->takeAt(0)) { if (item->widget()) item->widget()->deleteLater(); delete item; }
    const StatCritere c=m_criteres[index]; int total=0; for (int v:c.valeurs) total+=v;
    DonutChart *chart=new DonutChart(c.etiquettes,c.valeurs,this); m_grille->addWidget(chart,0,0,1,3,Qt::AlignHCenter);
    const QColor colors[]={QColor("#2E7D32"),QColor("#DC3C2D"),QColor("#1F4E79"),QColor("#E6A23C"),QColor("#7E57C2"),QColor("#607D8B")};
    for (int i=0;i<c.etiquettes.size();++i) {
        QLabel *swatch=new QLabel("●"); swatch->setStyleSheet(QString("color:%1; font-size:17px;").arg(colors[i%6].name()));
        int pct=total ? qRound(100.0*c.valeurs[i]/total) : 0;
        QLabel *label=new QLabel(c.etiquettes[i]); QLabel *value=new QLabel(QString("%1  •  %2 %").arg(c.valeurs[i]).arg(pct));
        value->setStyleSheet("font-weight:600; color:#1F2937;");
        m_grille->addWidget(swatch,i+1,0); m_grille->addWidget(label,i+1,1); m_grille->addWidget(value,i+1,2,Qt::AlignRight);
    }
    QLabel *totalLabel=new QLabel(QString("Total : %1 personne(s)").arg(total)); totalLabel->setStyleSheet("font-weight:600; color:#1F4E79; padding-top:8px;");
    m_grille->addWidget(totalLabel,c.etiquettes.size()+1,0,1,3);
    m_grille->setColumnStretch(1,1);
}

// ===== gsmartfirestation =====


#include <QDesktopServices>
#include <QFileDialog>
#include <QIcon>
#include <QLabel>
#include <QGroupBox>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPixmap>
#include <QProgressBar>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTableWidget>
#include <QUrl>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

GSmartFireStation::GSmartFireStation(QWidget *parent)
     : QMainWindow(parent), ui(new Ui::GSmartFireStation)
{
    ui->setupUi(this);
    repolish(this); // applique les class="..." du .ui au fichier style.qss

    setWindowIcon(QIcon(":/images/icon.png"));
    QPixmap logo(":/images/logo.png");
    ui->labelLogo->setPixmap(logo.scaledToWidth(192, Qt::SmoothTransformation));

    // Module Véhicules : interface du projet Vehicules_min (inchangée).
    setupVehicules();

    // Module Personnel : page du projet Personnel, insérée entre le Dashboard (0)
    // et les Véhicules. Le .ui ne contient que Dashboard (0) et Véhicules (1) :
    // après insertion, Personnel = 1 et Véhicules = 2.
    m_personnelPage = new PersonnelPage(this);
    ui->stackedWidget->insertWidget(1, m_personnelPage);
    repolish(m_personnelPage); // applique les class="..." de la page Personnel
}

GSmartFireStation::~GSmartFireStation()
{
    DatabaseManager::close();
    delete ui;
}

// Connexion Oracle + authentification + chargement initial (appelé par main).
bool GSmartFireStation::demarrer()
{
    // La connexion est facultative : si Oracle n'est pas disponible, l'application
    // reste utilisable (véhicules en mode démonstration, personnel vide).
    if (DatabaseManager::connectInteractive(this)) {
        // Oracle connecté : authentification obligatoire (écran du module Personnel).
        if (!m_personnelPage->connexion())
            return false;   // l'utilisateur a annulé : main quitte l'application
        Personnel u = m_personnelPage->utilisateurConnecte();
        ui->labelRole->setText(QString("Connecté : %1 (%2)").arg(u.nomComplet(), u.grade()));
        m_personnelPage->rafraichir();
    }
    chargerVehiculesExemple();
    refreshVehiculesTable();
    goToPage(0); // Dashboard : recharge le personnel, les alertes et la surcharge
    if (!DatabaseManager::isConnected())
        statusBar()->showMessage("Mode démonstration : aucune connexion Oracle active.", 10000);
    return true;
}

// ===================== NAVIGATION =====================
// Affiche la page d'index donné du QStackedWidget : 0 = Dashboard, 1 = Personnel, 2 = Véhicules
void GSmartFireStation::goToPage(int index)
{
    ui->stackedWidget->setCurrentIndex(index);
    statusBar()->clearMessage();

    // Le menu latéral montre la page affichée
    ui->btnNavDashboard->setChecked(index == 0);
    ui->btnNavPersonnel->setChecked(index == 1);
    ui->btnNavVehicules->setChecked(index == 2);

    if (index == 0) {
        // Le personnel a pu être modifié dans la page : on recharge avant de recalculer.
        chargerPersonnelDashboard();
        refreshDashboard(); // le Dashboard est recalculé à chaque affichage
    }
}

void GSmartFireStation::on_btnNavDashboard_clicked()
{
    goToPage(0);
}

void GSmartFireStation::on_btnNavPersonnel_clicked()
{
    goToPage(1);
}

void GSmartFireStation::on_btnNavVehicules_clicked()
{
    goToPage(2);
}

void GSmartFireStation::on_btnGererPersonnel_clicked()
{
    goToPage(1);
}

void GSmartFireStation::on_btnGererVehicules_clicked()
{
    goToPage(2);
}

void GSmartFireStation::on_btnRetourVehicule_clicked()
{
    goToPage(0);
}

void GSmartFireStation::on_btnQuitter_clicked()
{
    int reponse = QMessageBox::question(this, "Quitter", "Voulez-vous vraiment quitter l'application ?");
    if (reponse == QMessageBox::Yes)
        close();
}

// ===================== DASHBOARD =====================
// Met à jour une barre de progression et son texte "valeur/total"
void GSmartFireStation::majBarre(QProgressBar *barre, QLabel *label, int valeur, int total)
{
    label->setText(QString("%1/%2").arg(valeur).arg(total));
    if (total < 1)
        total = 1;
    barre->setMaximum(total);
    barre->setValue(valeur);
}

// Ajoute une ligne dans la liste des alertes (texte coloré)
void GSmartFireStation::ajouterAlerte(const QString &texte, const QString &couleur)
{
    QListWidgetItem *ligne = new QListWidgetItem("●  " + texte);
    ligne->setForeground(QColor(couleur));
    ui->listAlertes->addItem(ligne);
}

void GSmartFireStation::refreshDashboard()
{
    // --- Comptage du personnel ---
    int personnelDispo = 0;
    int personnelMission = 0;
    for (int i = 0; i < m_personnelDashboard.size(); i++) {
        if (m_personnelDashboard[i].disponibilite == "Disponible")
            personnelDispo++;
        else if (m_personnelDashboard[i].disponibilite == "En mission")
            personnelMission++;
    }

    // --- Comptage des véhicules ---
    int vehiculesDispo = 0;
    int vehiculesMaintenance = 0;
    for (int i = 0; i < m_vehicules.size(); i++) {
        if (m_vehicules[i].disponibilite() == "Disponible")
            vehiculesDispo++;
        else if (m_vehicules[i].disponibilite() == "En maintenance")
            vehiculesMaintenance++;
    }

    int nbPersonnel = m_personnelDashboard.size();
    int nbVehicules = m_vehicules.size();

    // --- Cartes du haut ---
    ui->labelNbPersonnel->setText(QString::number(nbPersonnel));
    ui->labelPersonnelDispoDetail->setText(QString("Disponibles : %1").arg(personnelDispo));
    ui->labelNbVehicules->setText(QString::number(nbVehicules));
    ui->labelVehiculesDispoDetail->setText(QString("Disponibles : %1").arg(vehiculesDispo));

    // --- Barres de disponibilité ---
    majBarre(ui->progressPersonnelDispo, ui->labelPersonnelDispoCount, personnelDispo, nbPersonnel);
    majBarre(ui->progressPersonnelMission, ui->labelPersonnelMissionCount, personnelMission, nbPersonnel);
    majBarre(ui->progressVehiculesDispo, ui->labelVehiculesDispoCount, vehiculesDispo, nbVehicules);
    majBarre(ui->progressVehiculesMaint, ui->labelVehiculesMaintCount, vehiculesMaintenance, nbVehicules);

    // --- Alertes ---
    ui->listAlertes->clear();
    int nbAlertes = 0;
    int seuil = ui->spinSeuil->value();
    for (int i = 0; i < m_personnelDashboard.size(); i++) {
        if (m_personnelDashboard[i].charge() >= seuil) {
            ajouterAlerte(QString("Personnel %1 : charge opérationnelle élevée (%2 %)")
                              .arg(m_personnelDashboard[i].id).arg(m_personnelDashboard[i].charge()), "#C00000");
            nbAlertes++;
        }
    }
    for (int i = 0; i < m_vehicules.size(); i++) {
        if (m_vehicules[i].disponibilite() != "En maintenance" && m_vehicules[i].maintenanceProche()) {
            ajouterAlerte(QString("Véhicule %1 : maintenance à prévoir (%2)")
                              .arg(m_vehicules[i].id())
                              .arg(m_vehicules[i].dateMaintenance().toString("dd/MM/yyyy")), "#C00000");
            nbAlertes++;
        }
    }
    ajouterAlerte("Préparer un véhicule pour une intervention", "#B45309");
    ui->labelNbAlertes->setText(QString::number(nbAlertes));

    refreshSurcharge();
}

// Bouton "Voir les statistiques" : statistiques des deux modules dans une seule fenêtre
void GSmartFireStation::on_btnVoirStats_clicked()
{
    StatCritere c1 = statPersonnelParDispo();
    c1.nom = "Personnel — Disponibilité";
    StatCritere c2 = statPersonnelParGrade();
    c2.nom = "Personnel — Grade";
    StatCritere c3 = statVehiculesParDispo();
    c3.nom = "Véhicules — Disponibilité";
    StatCritere c4 = statVehiculesParEtat();
    c4.nom = "Véhicules — État";

    QList<StatCritere> criteres;
    criteres << c1 << c2 << c3 << c4;
    StatsDialog fenetre("Statistiques de la station", criteres, this);
    fenetre.exec();
}

// ===================== OUTILS COMMUNS =====================

// Crée une case de tableau avec une couleur de texte (et éventuellement en gras)
QTableWidgetItem *GSmartFireStation::creerItem(const QString &texte, const QColor &couleur, bool gras)
{
    QTableWidgetItem *item = new QTableWidgetItem(texte);
    item->setForeground(couleur);
    if (gras) {
        QFont police = item->font();
        police.setBold(true);
        item->setFont(police);
    }
    return item;
}

// Change la "class" d'un label (pour qu'il prenne un autre style du fichier style.qss)
void GSmartFireStation::setLabelClass(QLabel *label, const QString &classe)
{
    label->setProperty("class", classe);
    label->style()->unpolish(label);
    label->style()->polish(label);
}

// Exporte en PDF les lignes visibles d'un tableau (donc le résultat d'une recherche / d'un tri)
void GSmartFireStation::exporterTableauPdf(QTableWidget *table, const QString &titre, const QString &nomFichier)
{
    QStringList entetes;
    for (int c = 0; c < table->columnCount(); c++)
        entetes << table->horizontalHeaderItem(c)->text();

    QList<QStringList> lignes;
    for (int r = 0; r < table->rowCount(); r++) {
        if (table->isRowHidden(r))
            continue;
        QStringList ligne;
        for (int c = 0; c < table->columnCount(); c++)
            ligne << table->item(r, c)->text();
        lignes << ligne;
    }
    if (lignes.size() == 0) {
        QMessageBox::information(this, "Export PDF", "Aucune donnée à exporter.");
        return;
    }

    QString dossier = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString chemin = QFileDialog::getSaveFileName(this, "Exporter en PDF",
                                                  dossier + "/" + nomFichier, "Fichiers PDF (*.pdf)");
    if (chemin == "")
        return; // l'utilisateur a annulé
    if (!chemin.endsWith(".pdf"))
        chemin += ".pdf";

    if (!creerPdfTableau(chemin, titre, entetes, lignes)) {
        QMessageBox::warning(this, "Export PDF", "Impossible de créer le fichier PDF.");
        return;
    }
    statusBar()->showMessage("PDF exporté : " + chemin, 6000);
    int reponse = QMessageBox::question(this, "Export PDF",
                                        "Le PDF a été créé.\nVoulez-vous l'ouvrir maintenant ?");
    if (reponse == QMessageBox::Yes)
        QDesktopServices::openUrl(QUrl::fromLocalFile(chemin));
}

// ===== personnel (dashboard + surcharge) =====

#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>

// =====================================================================
//   PERSONNEL : données du Dashboard + détection de surcharge
//   (la page de gestion complète est dans personnel.cpp — module du camarade)
// =====================================================================

// Lit le personnel et le nombre de missions en cours de chacun (pour la charge).
void GSmartFireStation::chargerPersonnelDashboard()
{
    m_personnelDashboard.clear();
    if (!DatabaseManager::isConnected())
        return;   // mode démonstration : pas de données personnel

    QSqlQuery q(DatabaseManager::database());
    q.prepare("SELECT p.id_personnel, p.nom, p.prenom, p.grade, p.disponibilite, "
              "(SELECT COUNT(*) FROM mission_personnel mp JOIN mission m "
              "ON m.id_mission=mp.id_mission WHERE mp.id_personnel=p.id_personnel "
              "AND UPPER(m.etat) IN ('EN COURS','EN_COURS','EN COURS')) "
              "FROM personnel p ORDER BY p.id_personnel");
    if (!q.exec()) {
        QMessageBox::warning(this, "Erreur Oracle",
            "Impossible de charger le personnel du Dashboard : " + q.lastError().text());
        return;
    }
    while (q.next()) {
        PersonnelDashboard l;
        l.id = QString("P%1").arg(q.value(0).toInt(), 3, 10, QChar('0'));
        l.nomComplet = q.value(2).toString() + " " + q.value(1).toString();
        l.grade = q.value(3).toString();
        l.disponibilite = q.value(4).toString();
        l.missionsEnCours = q.value(5).toInt();
        m_personnelDashboard.append(l);
    }
}

// Métier innovant : détection de surcharge (panneau du Dashboard)
void GSmartFireStation::refreshSurcharge()
{
    int seuil = ui->spinSeuil->value();

    int plusCharge = -1;  // numéro du personnel le plus chargé (parmi ceux en surcharge)
    int nbSurcharge = 0;
    int chargeMax = 0;
    for (int i = 0; i < m_personnelDashboard.size(); i++) {
        int c = m_personnelDashboard[i].charge();
        if (c > chargeMax)
            chargeMax = c;
        if (c >= seuil) {
            nbSurcharge++;
            if (plusCharge < 0 || c > m_personnelDashboard[plusCharge].charge())
                plusCharge = i;
        }
    }

    if (plusCharge < 0) { // personne en surcharge
        ui->labelSurchargeNom->setText("Aucun personnel en surcharge");
        ui->labelSurchargeCharge->setText(QString("Charge maximale : %1 %").arg(chargeMax));
        setLabelClass(ui->labelSurchargeCharge, "cardDetail");
        ui->labelSurchargeEtat->setText("✔ Charge opérationnelle normale");
        setLabelClass(ui->labelSurchargeEtat, "textSuccessBold");
        ui->btnDetailsSurcharge->setEnabled(false);
        return;
    }

    const PersonnelDashboard &p = m_personnelDashboard[plusCharge];
    ui->labelSurchargeNom->setText(p.id + " — " + p.nomComplet);
    ui->labelSurchargeCharge->setText(QString("Charge actuelle : %1 %").arg(p.charge()));
    setLabelClass(ui->labelSurchargeCharge, "textDanger");
    QString etat = "⚠ Surcharge détectée";
    if (nbSurcharge > 1)
        etat += QString(" (+%1 autre(s))").arg(nbSurcharge - 1);
    ui->labelSurchargeEtat->setText(etat);
    setLabelClass(ui->labelSurchargeEtat, "textDangerBold");
    ui->btnDetailsSurcharge->setEnabled(true);
}

void GSmartFireStation::on_spinSeuil_valueChanged(int)
{
    refreshDashboard(); // le seuil a changé : on recalcule alertes + surcharge
}

void GSmartFireStation::on_btnDetailsSurcharge_clicked()
{
    int seuil = ui->spinSeuil->value();
    QString texte = "";
    int nb = 0;
    for (int i = 0; i < m_personnelDashboard.size(); i++) {
        const PersonnelDashboard &p = m_personnelDashboard[i];
        if (p.charge() >= seuil) {
            nb++;
            texte += QString("• %1 — %2 (%3) : %4 mission(s), charge %5 %\n")
                         .arg(p.id).arg(p.nomComplet).arg(p.grade)
                         .arg(p.missionsEnCours).arg(p.charge());
        }
    }
    if (nb == 0)
        return;

    QMessageBox boite(this);
    boite.setIcon(QMessageBox::Warning);
    boite.setWindowTitle("Personnel en surcharge");
    boite.setText(QString("%1 personnel(s) dépassent le seuil de %2 %.").arg(nb).arg(seuil));
    boite.setInformativeText(texte + "\nPensez à réaffecter une partie de leurs missions.");
    boite.exec();
}

// ------------------------------------------------------------------ statistiques
StatCritere GSmartFireStation::statPersonnelParGrade()
{
    QStringList reelles;
    for (int i = 0; i < m_personnelDashboard.size(); i++)
        reelles << m_personnelDashboard[i].grade;
    return compterParValeur("Grade", Personnel::grades(), reelles);
}

StatCritere GSmartFireStation::statPersonnelParDispo()
{
    QStringList reelles;
    for (int i = 0; i < m_personnelDashboard.size(); i++)
        reelles << m_personnelDashboard[i].disponibilite;
    return compterParValeur("Disponibilité", Personnel::disponibilites(), reelles);
}

// ===== gsmartfirestation_vehicules =====

#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QDateEdit>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSizePolicy>
#include <QScrollArea>
#include <QPushButton>

#include <QHeaderView>
#include <QMessageBox>
#include <QStatusBar>
#include <QTableWidgetItem>
#include <QSqlQuery>
#include <QSqlError>

// =====================================================================
//                        MODULE VÉHICULES
// =====================================================================

void GSmartFireStation::setupVehicules()
{
    m_vehiculePropose = -1;
    ui->frameMission->hide();

    // Colonnes du tableau : ID étroite, les autres se partagent la place
    QHeaderView *entete = ui->tableWidgetVehicules->horizontalHeader();
    entete->setSectionResizeMode(QHeaderView::Stretch);
    entete->setSectionResizeMode(0, QHeaderView::Fixed);
    entete->resizeSection(0, 70);
    entete->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_vehiculeEditIndex = -1;
    m_formVehicule = new QGroupBox("Fiche du véhicule", this);
    m_formVehicule->setStyleSheet("QGroupBox { border: 1px solid #D7DEE8; border-radius: 8px; margin-top: 10px; padding: 10px; font-weight: 600; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; color: #1F4E79; }");
    m_vehicleId = new QLabel(prochainIdVehicule());
    m_vehicleImmat = new QLineEdit; m_vehicleImmat->setPlaceholderText("ex. 123 TU 456");
    m_vehicleType = new QComboBox; m_vehicleType->addItems(Vehicule::types());
    m_vehicleModele = new QLineEdit; m_vehicleModele->setPlaceholderText("Modèle / marque");
    m_vehicleEtat = new QComboBox; m_vehicleEtat->addItems(Vehicule::etats());
    m_vehicleDispo = new QComboBox; m_vehicleDispo->addItems(Vehicule::disponibilites());
    m_vehicleDate = new QDateEdit(QDate::currentDate().addMonths(3)); m_vehicleDate->setCalendarPopup(true); m_vehicleDate->setDisplayFormat("dd/MM/yyyy");
    // Permanent compact horizontal editor to preserve the table's height.
    QGridLayout *form = new QGridLayout(m_formVehicule);
    form->setContentsMargins(8,12,8,8); form->setHorizontalSpacing(10); form->setVerticalSpacing(5);
    auto addVehicleField = [&](int row, int col, const QString &name, QWidget *field) {
        QLabel *label = new QLabel(name); label->setStyleSheet("font-weight: normal;");
        QVBoxLayout *cell = new QVBoxLayout; cell->setContentsMargins(0,0,0,0); cell->setSpacing(2); cell->addWidget(label); cell->addWidget(field);
        QWidget *wrap = new QWidget; wrap->setLayout(cell); form->addWidget(wrap,row,col);
    };
    addVehicleField(0,0,"ID",m_vehicleId); addVehicleField(0,1,"Immatriculation",m_vehicleImmat); addVehicleField(0,2,"Type",m_vehicleType); addVehicleField(0,3,"Modèle",m_vehicleModele);
    addVehicleField(1,0,"État",m_vehicleEtat); addVehicleField(1,1,"Disponibilité",m_vehicleDispo); addVehicleField(1,2,"Date maintenance",m_vehicleDate);
    for (int c=0;c<4;++c) form->setColumnStretch(c,1);
    QPushButton *save = new QPushButton("Enregistrer"); save->setStyleSheet("QPushButton { background:#1F4E79; color:white; border-radius:5px; padding:6px 10px; font-weight:600; }");
    QPushButton *clear = new QPushButton("Effacer"); QHBoxLayout *actions = new QHBoxLayout; actions->addWidget(save); actions->addWidget(clear); form->addLayout(actions,1,3);
    ui->verticalLayoutVehiculesLeft->addWidget(m_formVehicule);
    // Keep the compact editor permanently visible below the table.
    m_formVehicule->show();
    QHBoxLayout *crud = ui->horizontalLayoutCrudVehicule;
    crud->removeWidget(ui->btnAjouterVehicule); crud->removeWidget(ui->btnModifierVehicule); crud->removeWidget(ui->btnSupprimerVehicule);
    ui->verticalLayoutVehiculesLeft->removeItem(crud);
    delete crud;
    QWidget *actionsCrud = new QWidget;
    QHBoxLayout *crudLayout = new QHBoxLayout(actionsCrud);
    crudLayout->setContentsMargins(0, 8, 0, 0);
    crudLayout->setSpacing(10);
    crudLayout->addWidget(ui->btnAjouterVehicule);
    crudLayout->addWidget(ui->btnModifierVehicule);
    crudLayout->addWidget(ui->btnSupprimerVehicule);
    crudLayout->addStretch(1);
    // Actions remain visible immediately below the vehicle table.
    ui->verticalLayoutVehiculesLeft->insertWidget(ui->verticalLayoutVehiculesLeft->indexOf(m_formVehicule), actionsCrud);
    connect(save, SIGNAL(clicked()), this, SLOT(enregistrerVehiculeInline())); connect(clear, SIGNAL(clicked()), this, SLOT(annulerVehiculeInline()));
    ui->btnStatsVehicule->hide(); ui->progressStatVehDispo->hide(); ui->labelStatVehDispo->hide(); ui->labelStatVehMaint->hide();
    m_vehiculeStatsChart = new PieChartWidget("VÉHICULES", ui->frameStatsVehicules);
    m_vehiculeStatsChart->setMinimumHeight(390);
    QScrollArea *statsScroll = new QScrollArea(ui->frameStatsVehicules);
    statsScroll->setWidgetResizable(true);
    statsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    statsScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    statsScroll->setFrameShape(QFrame::NoFrame);
    QWidget *statsContent = new QWidget;
    QVBoxLayout *statsContentLayout = new QVBoxLayout(statsContent);
    statsContentLayout->setContentsMargins(4, 4, 4, 4);
    statsContentLayout->addWidget(m_vehiculeStatsChart);
    statsScroll->setWidget(statsContent);
    ui->verticalLayoutStatsVehicules->insertWidget(1, statsScroll, 1);
    ui->frameStatsVehicules->setMinimumWidth(285);
    ui->frameStatsVehicules->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
}

// Données de démonstration (à remplacer plus tard par la base de données)
void GSmartFireStation::chargerVehiculesExemple()
{
    m_vehicules.clear();
    if (DatabaseManager::isConnected()) {
        QSqlQuery q(DatabaseManager::database());
        q.prepare("SELECT id_vehicule, immatriculation, type_vehicule, modele, etat, disponibilite, date_maintenance "
                  "FROM vehicule ORDER BY id_vehicule");
        if (q.exec()) {
            while (q.next()) {
                QString id = QString("V%1").arg(q.value(0).toInt(), 3, 10, QChar('0'));
                QString type = q.value(2).toString();
                if (type.compare("Camion-citerne", Qt::CaseInsensitive) == 0) type = "Citerne";
                QString etat = q.value(4).toString();
                if (etat.compare("Operationnel", Qt::CaseInsensitive) == 0 ||
                    etat.compare("Opérationnel", Qt::CaseInsensitive) == 0) etat = "Bon";
                if (!Vehicule::types().contains(type)) type = "Camion";
                if (!Vehicule::etats().contains(etat)) etat = "À contrôler";
                QDate date = q.value(6).toDate();
                if (!date.isValid()) date = QDate::currentDate().addMonths(3);
                m_vehicules.append(Vehicule(id, q.value(1).toString(), type,
                    q.value(3).toString(), etat, q.value(5).toString(), date));
            }
            return;
        }
        QMessageBox::warning(this, "Erreur Oracle",
            "Impossible de charger VEHICULE : " + q.lastError().text());
    }

    // Mode démonstration si Oracle n'est pas connecté.
    m_vehicules.append(Vehicule("V001", "123 TU 456", "Fourgon", "Renault", "Bon", "Disponible", QDate(2026, 11, 10)));
    m_vehicules.append(Vehicule("V002", "124 TU 457", "Camion", "Iveco", "Bon", "En mission", QDate(2026, 10, 22)));
    m_vehicules.append(Vehicule("V003", "125 TU 458", "Ambulance", "Mercedes", "À contrôler", "Disponible", QDate(2026, 10, 2)));
    m_vehicules.append(Vehicule("V004", "126 TU 459", "Citerne", "MAN", "Bon", "Disponible", QDate(2026, 12, 18)));
    m_vehicules.append(Vehicule("V005", "127 TU 460", "Ambulance", "Renault", "Bon", "Disponible", QDate(2027, 1, 15)));
    m_vehicules.append(Vehicule("V006", "128 TU 461", "Échelle", "Iveco", "À contrôler", "En maintenance", QDate(2026, 10, 29)));
    m_vehicules.append(Vehicule("V007", "129 TU 462", "Fourgon", "MAN", "Bon", "En maintenance", QDate(2026, 10, 12)));
}

// Numéro de la ligne sélectionnée dans le tableau (-1 si aucune)
int GSmartFireStation::vehiculeSelectionne()
{
    int ligne = ui->tableWidgetVehicules->currentRow();
    if (ligne < 0 || ligne >= m_vehicules.size())
        return -1;
    return ligne;
}

// Prochain ID libre : V001, V002, ...
QString GSmartFireStation::prochainIdVehicule()
{
    int max = 0;
    for (int i = 0; i < m_vehicules.size(); i++) {
        int numero = m_vehicules[i].id().mid(1).toInt();
        if (numero > max)
            max = numero;
    }
    return QString("V%1").arg(max + 1, 3, 10, QChar('0'));
}

// ------------------------------------------------------------------ affichage
void GSmartFireStation::refreshVehiculesTable()
{
    int ligneCourante = ui->tableWidgetVehicules->currentRow();
    QColor noir("#1F1F1F");

    ui->tableWidgetVehicules->setRowCount(m_vehicules.size());
    for (int r = 0; r < m_vehicules.size(); r++) {
        Vehicule v = m_vehicules[r];

        // Couleur de l'état
        QColor couleurEtat = noir;
        bool etatGras = false;
        if (v.etat() == "À contrôler") {
            couleurEtat = QColor("#B45309");
            etatGras = true;
        } else if (v.etat() == "Hors service") {
            couleurEtat = QColor("#C00000");
            etatGras = true;
        }

        // Couleur de la disponibilité
        QColor couleurDispo("#C00000"); // En maintenance
        if (v.disponibilite() == "Disponible")
            couleurDispo = QColor("#2E7D32");
        else if (v.disponibilite() == "En mission")
            couleurDispo = QColor("#B45309");

        // Date de maintenance : orange si proche, rouge si dépassée
        QColor couleurDate = noir;
        bool dateGras = false;
        if (v.disponibilite() != "En maintenance" && v.maintenanceProche()) {
            dateGras = true;
            if (v.dateMaintenance() < QDate::currentDate())
                couleurDate = QColor("#C00000");
            else
                couleurDate = QColor("#B45309");
        }

        ui->tableWidgetVehicules->setItem(r, 0, creerItem(v.id(), noir, false));
        ui->tableWidgetVehicules->setItem(r, 1, creerItem(v.immatriculation(), noir, false));
        ui->tableWidgetVehicules->setItem(r, 2, creerItem(v.type(), noir, false));
        ui->tableWidgetVehicules->setItem(r, 3, creerItem(v.modele(), noir, false));
        ui->tableWidgetVehicules->setItem(r, 4, creerItem(v.etat(), couleurEtat, etatGras));
        ui->tableWidgetVehicules->setItem(r, 5, creerItem(v.disponibilite(), couleurDispo, true));
        ui->tableWidgetVehicules->setItem(r, 6, creerItem(v.dateMaintenance().toString("dd/MM/yyyy"), couleurDate, dateGras));
    }
    if (ligneCourante >= 0 && ligneCourante < m_vehicules.size())
        ui->tableWidgetVehicules->selectRow(ligneCourante);

    filtrerVehicules();

    // Panneau "Statistiques véhicules"
    int dispo = 0;
    int maintenance = 0;
    for (int i = 0; i < m_vehicules.size(); i++) {
        if (m_vehicules[i].disponibilite() == "Disponible")
            dispo++;
        else if (m_vehicules[i].disponibilite() == "En maintenance")
            maintenance++;
    }
    int total = m_vehicules.size();
    int maximum = total;
    if (maximum < 1)
        maximum = 1;
    ui->labelStatVehDispo->setText(QString("Disponibles : %1 / %2").arg(dispo).arg(total));
    ui->progressStatVehDispo->setMaximum(maximum); ui->progressStatVehDispo->setValue(dispo);
    ui->labelStatVehMaint->setText(QString("En maintenance : %1").arg(maintenance));
    QStringList types = Vehicule::types(); QStringList labels; QList<int> values;
    for (int t=0; t<types.size(); ++t) { int count=0; for (int i=0;i<m_vehicules.size();++i) if (m_vehicules[i].type()==types[t]) ++count; labels << types[t]; values << count; }
    static_cast<PieChartWidget*>(m_vehiculeStatsChart)->setData(labels, values);
    refreshVehiculePropose();
}

// ------------------------------------------------------------------ CRUD
void GSmartFireStation::on_btnAjouterVehicule_clicked()
{
    annulerVehiculeInline(); m_vehiculeEditIndex = -1; m_vehicleId->setText(prochainIdVehicule());
    m_formVehicule->setTitle("Fiche du véhicule — ajoutez ou modifiez les champs ci-dessous"); m_formVehicule->show(); m_vehicleImmat->setFocus();
}

void GSmartFireStation::on_btnModifierVehicule_clicked()
{
    int i = vehiculeSelectionne();
    if (i < 0) { QMessageBox::information(this, "Modifier un véhicule", "Sélectionnez d'abord un véhicule dans la liste."); return; }
    const Vehicule v=m_vehicules[i]; m_vehiculeEditIndex=i; m_vehicleId->setText(v.id());
    m_vehicleImmat->setText(v.immatriculation()); m_vehicleType->setCurrentText(v.type()); m_vehicleModele->setText(v.modele());
    m_vehicleEtat->setCurrentText(v.etat()); m_vehicleDispo->setCurrentText(v.disponibilite()); m_vehicleDate->setDate(v.dateMaintenance());
    m_formVehicule->setTitle("Modification du véhicule " + v.id());
    m_formVehicule->show();
}

void GSmartFireStation::enregistrerVehiculeInline()
{
    QString immat = m_vehicleImmat->text().trimmed().toUpper();
    QString modele = m_vehicleModele->text().trimmed();
    if (immat.isEmpty() || modele.isEmpty()) {
        QMessageBox::warning(this, "Saisie incorrecte", "L'immatriculation et le modèle sont obligatoires.");
        return;
    }
    for (int i = 0; i < m_vehicules.size(); ++i) {
        if (i != m_vehiculeEditIndex && m_vehicules[i].immatriculation().compare(immat, Qt::CaseInsensitive) == 0) {
            QMessageBox::warning(this, "Doublon", "Cette immatriculation existe déjà.");
            return;
        }
    }

    const bool editing = m_vehiculeEditIndex >= 0 && m_vehiculeEditIndex < m_vehicules.size();
    if (DatabaseManager::isConnected()) {
        QString typeDb = m_vehicleType->currentText();
        if (typeDb == "Citerne") typeDb = "Camion-citerne";
        QString etatDb = m_vehicleEtat->currentText();
        if (etatDb == "Bon") etatDb = "Operationnel";

        QSqlQuery q(DatabaseManager::database());
        if (editing) {
            q.prepare("UPDATE vehicule SET immatriculation=:immat, type_vehicule=:type, modele=:modele, "
                      "etat=:etat, disponibilite=:dispo, date_maintenance=:date WHERE id_vehicule=:id");
            q.bindValue(":id", m_vehicules[m_vehiculeEditIndex].id().mid(1).toInt());
        } else {
            q.prepare("INSERT INTO vehicule (immatriculation, type_vehicule, modele, etat, disponibilite, date_maintenance) "
                      "VALUES (:immat, :type, :modele, :etat, :dispo, :date)");
        }
        q.bindValue(":immat", immat);
        q.bindValue(":type", typeDb);
        q.bindValue(":modele", modele);
        q.bindValue(":etat", etatDb);
        q.bindValue(":dispo", m_vehicleDispo->currentText());
        q.bindValue(":date", m_vehicleDate->date());
        if (!q.exec()) {
            QMessageBox::critical(this, "Erreur Oracle", "Enregistrement du véhicule impossible : " + q.lastError().text());
            return;
        }
        chargerVehiculesExemple();
        m_vehiculeEditIndex = -1;
        refreshVehiculesTable();
        m_formVehicule->setTitle("Fiche du véhicule — ajoutez ou modifiez les champs ci-dessous");
        statusBar()->showMessage(editing ? "Véhicule modifié dans Oracle." : "Véhicule ajouté dans Oracle.", 5000);
        return;
    }

    Vehicule v(editing ? m_vehicules[m_vehiculeEditIndex].id() : prochainIdVehicule(),
        immat, m_vehicleType->currentText(), modele, m_vehicleEtat->currentText(),
        m_vehicleDispo->currentText(), m_vehicleDate->date());
    if (editing) m_vehicules[m_vehiculeEditIndex] = v;
    else { m_vehicules.append(v); m_vehiculeEditIndex = m_vehicules.size() - 1; }
    refreshVehiculesTable();
    ui->tableWidgetVehicules->selectRow(m_vehiculeEditIndex);
    statusBar()->showMessage(editing ? "Véhicule modifié (mode démonstration)." : "Véhicule ajouté (mode démonstration).", 4000);
    m_formVehicule->setTitle("Fiche du véhicule — ajoutez ou modifiez les champs ci-dessous");
}

void GSmartFireStation::annulerVehiculeInline()
{
    m_vehiculeEditIndex=-1; m_vehicleId->setText(prochainIdVehicule()); m_vehicleImmat->clear(); m_vehicleModele->clear();
    m_vehicleType->setCurrentIndex(0); m_vehicleEtat->setCurrentIndex(0); m_vehicleDispo->setCurrentText("Disponible"); m_vehicleDate->setDate(QDate::currentDate().addMonths(3));
    m_formVehicule->setTitle("Fiche du véhicule — ajoutez ou modifiez les champs ci-dessous");
    m_formVehicule->show();
}

void GSmartFireStation::on_btnSupprimerVehicule_clicked()
{
    int i = vehiculeSelectionne();
    if (i < 0) {
        QMessageBox::information(this, "Supprimer un véhicule",
                                 "Sélectionnez d'abord un véhicule dans la liste.");
        return;
    }
    int reponse = QMessageBox::question(this, "Supprimer un véhicule",
                                        "Voulez-vous vraiment supprimer le véhicule "
                                        + m_vehicules[i].immatriculation() + " ?");
    if (reponse != QMessageBox::Yes)
        return;

    if (DatabaseManager::isConnected()) {
        QSqlQuery q(DatabaseManager::database());
        q.prepare("DELETE FROM vehicule WHERE id_vehicule=:id");
        q.bindValue(":id", m_vehicules[i].id().mid(1).toInt());
        if (!q.exec()) {
            QMessageBox::critical(this, "Erreur Oracle", "Suppression impossible : " + q.lastError().text());
            return;
        }
        chargerVehiculesExemple();
    } else {
        m_vehicules.removeAt(i);
    }
    ui->tableWidgetVehicules->setCurrentCell(-1, -1);
    refreshVehiculesTable();
    statusBar()->showMessage("Véhicule supprimé.", 5000);
}

void GSmartFireStation::on_tableWidgetVehicules_cellDoubleClicked(int, int)
{
    on_btnModifierVehicule_clicked(); // double-clic = modifier
}

// ------------------------------------------------------------------ recherche
// Cache les lignes qui ne contiennent pas le texte cherché (immatriculation, type, modèle ou ID)
void GSmartFireStation::filtrerVehicules()
{
    QString texte = ui->lineEditRechercheVehicule->text().trimmed();
    int visibles = 0;
    for (int r = 0; r < m_vehicules.size(); r++) {
        Vehicule v = m_vehicules[r];
        bool trouve = texte == ""
                      || v.immatriculation().contains(texte, Qt::CaseInsensitive)
                      || v.type().contains(texte, Qt::CaseInsensitive)
                      || v.modele().contains(texte, Qt::CaseInsensitive)
                      || v.id().contains(texte, Qt::CaseInsensitive);
        ui->tableWidgetVehicules->setRowHidden(r, !trouve);
        if (trouve)
            visibles++;
    }
    if (texte != "")
        statusBar()->showMessage(QString("%1 résultat(s)").arg(visibles), 4000);
    else
        statusBar()->clearMessage();
}

void GSmartFireStation::on_btnRechercherVehicule_clicked()
{
    filtrerVehicules();
}

void GSmartFireStation::on_lineEditRechercheVehicule_textChanged(const QString &)
{
    filtrerVehicules(); // la recherche se met à jour pendant la frappe
}

// ------------------------------------------------------------------ tri (tri à bulles)
// Retourne vrai si 'a' doit passer après 'b' selon le critère choisi
bool GSmartFireStation::doitEchangerVehicules(Vehicule a, Vehicule b, int critere)
{
    if (critere == 0) // État : Bon, À contrôler, Hors service
        return Vehicule::etats().indexOf(a.etat()) > Vehicule::etats().indexOf(b.etat());
    // Type : Fourgon, Camion, Ambulance, Citerne, Échelle
    return Vehicule::types().indexOf(a.type()) > Vehicule::types().indexOf(b.type());
}

void GSmartFireStation::on_btnTrierVehicule_clicked()
{
    int critere = ui->comboTriVehicule->currentIndex();
    int n = m_vehicules.size();
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            if (doitEchangerVehicules(m_vehicules[j], m_vehicules[j + 1], critere)) {
                Vehicule temp = m_vehicules[j];
                m_vehicules[j] = m_vehicules[j + 1];
                m_vehicules[j + 1] = temp;
            }
        }
    }
    ui->tableWidgetVehicules->setCurrentCell(-1, -1);
    refreshVehiculesTable();
    statusBar()->showMessage("Liste triée.", 4000);
}

// ------------------------------------------------------------------ export PDF
void GSmartFireStation::on_btnExportVehicule_clicked()
{
    exporterTableauPdf(ui->tableWidgetVehicules, "Liste des véhicules", "vehicules.pdf");
}

// ------------------------------------------------------------------ statistiques
StatCritere GSmartFireStation::statVehiculesParEtat()
{
    QStringList reelles;
    for (int i = 0; i < m_vehicules.size(); i++)
        reelles << m_vehicules[i].etat();
    return compterParValeur("État", Vehicule::etats(), reelles);
}

StatCritere GSmartFireStation::statVehiculesParDispo()
{
    QStringList reelles;
    for (int i = 0; i < m_vehicules.size(); i++)
        reelles << m_vehicules[i].disponibilite();
    return compterParValeur("Disponibilité", Vehicule::disponibilites(), reelles);
}

void GSmartFireStation::on_btnStatsVehicule_clicked()
{
    // Les statistiques sont affichées en permanence dans le panneau latéral.
}

// ------------------------------------------------------------------ métier : choix adapté à la mission
// Propose un véhicule DISPONIBLE, en BON ÉTAT, dont le type correspond à l'intervention.
// On teste les types par ordre de préférence ; parmi les candidats on prend celui dont
// la prochaine maintenance est la plus lointaine.
void GSmartFireStation::refreshVehiculePropose()
{
    m_vehiculePropose = -1;
    ui->frameMission->hide();
    QStringList typesPreferes = Vehicule::typesPourIntervention(ui->comboIntervention->currentText());

    for (int t = 0; t < typesPreferes.size() && m_vehiculePropose < 0; t++) {
        int meilleur = -1;
        for (int i = 0; i < m_vehicules.size(); i++) {
            Vehicule v = m_vehicules[i];
            if (v.type() == typesPreferes[t] && v.disponibilite() == "Disponible" && v.etat() == "Bon") {
                if (meilleur < 0 || v.dateMaintenance() > m_vehicules[meilleur].dateMaintenance())
                    meilleur = i;
            }
        }
        m_vehiculePropose = meilleur; // reste -1 si aucun véhicule de ce type
    }

    if (m_vehiculePropose < 0) {
        ui->labelVehiculePropose->setText("Aucun véhicule adapté");
        ui->labelVehiculeInfo->setText("Aucun véhicule disponible en bon état pour cette intervention.");
        setLabelClass(ui->labelVehiculeInfo, "textWarning");
        ui->btnUtiliserVehicule->setEnabled(false);
        return;
    }

    Vehicule v = m_vehicules[m_vehiculePropose];
    ui->labelVehiculePropose->setText(v.id() + " — " + v.type() + " " + v.modele());
    ui->labelVehiculeInfo->setText(v.disponibilite() + " • État : " + v.etat());
    setLabelClass(ui->labelVehiculeInfo, "textSuccess");
    ui->btnUtiliserVehicule->setEnabled(true);
}

void GSmartFireStation::on_comboIntervention_currentIndexChanged(int)
{
    refreshVehiculePropose();
}

void GSmartFireStation::on_btnUtiliserVehicule_clicked()
{
    if (m_vehiculePropose < 0)
        return;

    Vehicule v = m_vehicules[m_vehiculePropose];
    QString intervention = ui->comboIntervention->currentText();
    int reponse = QMessageBox::question(this, "Affecter le véhicule",
                                        "Affecter " + v.id() + " (" + v.type() + " " + v.modele()
                                        + ") à l'intervention « " + intervention + " » ?");
    if (reponse != QMessageBox::Yes)
        return;

    m_vehicules[m_vehiculePropose].setDisponibilite("En mission");
    refreshVehiculesTable();
    statusBar()->showMessage("Véhicule " + v.id() + " affecté : " + intervention + ".", 6000);
}


// ===================== MODULE PERSONNEL (integrated) =====================


#include <QComboBox>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QPainter>
#include <QPdfWriter>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableView>
#include <QTextDocument>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariant>

static QString s_erreur; // dernier message d'erreur SQL

// =====================================================================
//  2. PERSONNEL (modèle : entité + SQL + métiers + contrôle de saisie)
// =====================================================================
Personnel::Personnel()
{
    m_id = 0;
}

Personnel::Personnel(int id, const QString &nom, const QString &prenom, const QString &grade,
                     const QString &poste, const QString &telephone, const QString &email,
                     const QString &motDePasse, const QString &questionSecrete,
                     const QString &reponseSecrete, const QString &disponibilite)
{
    m_id = id;
    m_nom = nom;
    m_prenom = prenom;
    m_grade = grade;
    m_poste = poste;
    m_telephone = telephone;
    m_email = email;
    m_motDePasse = motDePasse;
    m_questionSecrete = questionSecrete;
    m_reponseSecrete = reponseSecrete;
    m_disponibilite = disponibilite;
}

// ------------------------------------------------------------------
//  CREATE : l'identifiant est généré par la séquence seq_personnel
// ------------------------------------------------------------------
bool Personnel::ajouter() const
{
    QSqlQuery query(DatabaseManager::database());
    query.prepare("INSERT INTO personnel (id_personnel, nom, prenom, grade, poste, telephone, email, "
                  "mot_de_passe, question_secrete, reponse_secrete, disponibilite) "
                  "VALUES (seq_personnel.NEXTVAL, :nom, :prenom, :grade, :poste, :telephone, :email, "
                  ":mdp, :question, :reponse, :dispo)");
    query.bindValue(":nom", m_nom);
    query.bindValue(":prenom", m_prenom);
    query.bindValue(":grade", m_grade);
    query.bindValue(":poste", m_poste);
    query.bindValue(":telephone", m_telephone);
    query.bindValue(":email", m_email);
    query.bindValue(":mdp", hacher(m_motDePasse));
    query.bindValue(":question", m_questionSecrete);
    query.bindValue(":reponse", hacher(normaliser(m_reponseSecrete)));
    query.bindValue(":dispo", m_disponibilite);

    if (!query.exec()) {
        s_erreur = query.lastError().text();
        qDebug() << "Erreur ajouter :" << s_erreur;
        return false;
    }
    return true;
}

// ------------------------------------------------------------------
//  UPDATE : le mot de passe et la réponse ne changent que s'ils sont renseignés
// ------------------------------------------------------------------
bool Personnel::modifier() const
{
    QString sql = "UPDATE personnel SET nom = :nom, prenom = :prenom, grade = :grade, poste = :poste, "
                  "telephone = :telephone, email = :email, question_secrete = :question, "
                  "disponibilite = :dispo";
    if (!m_motDePasse.isEmpty())
        sql += ", mot_de_passe = :mdp";
    if (!m_reponseSecrete.isEmpty())
        sql += ", reponse_secrete = :reponse";
    sql += " WHERE id_personnel = :id";

    QSqlQuery query(DatabaseManager::database());
    query.prepare(sql);
    query.bindValue(":nom", m_nom);
    query.bindValue(":prenom", m_prenom);
    query.bindValue(":grade", m_grade);
    query.bindValue(":poste", m_poste);
    query.bindValue(":telephone", m_telephone);
    query.bindValue(":email", m_email);
    query.bindValue(":question", m_questionSecrete);
    query.bindValue(":dispo", m_disponibilite);
    if (!m_motDePasse.isEmpty())
        query.bindValue(":mdp", hacher(m_motDePasse));
    if (!m_reponseSecrete.isEmpty())
        query.bindValue(":reponse", hacher(normaliser(m_reponseSecrete)));
    query.bindValue(":id", m_id);

    if (!query.exec()) {
        s_erreur = query.lastError().text();
        qDebug() << "Erreur modifier :" << s_erreur;
        return false;
    }
    return true;
}

// ------------------------------------------------------------------
//  DELETE
// ------------------------------------------------------------------
bool Personnel::supprimer(int id)
{
    QSqlQuery query(DatabaseManager::database());
    query.prepare("DELETE FROM personnel WHERE id_personnel = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        s_erreur = query.lastError().text();
        qDebug() << "Erreur supprimer :" << s_erreur;
        return false;
    }
    return true;
}

// ------------------------------------------------------------------
//  READ : modèle pour le tableau, avec recherche (ID, nom, prénom ou grade) et tri
//  tri : 0 = ID, 1 = grade (du plus élevé au moins élevé), 2 = disponibilité, 3 = nom
// ------------------------------------------------------------------
QSqlQueryModel *Personnel::afficher(const QString &recherche, int tri)
{
    QString ordre = "ORDER BY p.id_personnel";
    if (tri == 1)
        ordre = "ORDER BY CASE p.grade WHEN 'Colonel' THEN 1 WHEN 'Commandant' THEN 2 "
                "WHEN 'Capitaine' THEN 3 WHEN 'Lieutenant' THEN 4 WHEN 'Adjudant' THEN 5 "
                "WHEN 'Sergent' THEN 6 WHEN 'Caporal' THEN 7 ELSE 8 END, p.nom";
    else if (tri == 2)
        ordre = "ORDER BY CASE p.disponibilite WHEN 'Disponible' THEN 1 WHEN 'En mission' THEN 2 "
                "ELSE 3 END, p.nom";
    else if (tri == 3)
        ordre = "ORDER BY LOWER(p.nom), LOWER(p.prenom)";

    QString sql =
        "SELECT p.id_personnel, p.nom, p.prenom, p.grade, p.poste, p.telephone, p.disponibilite "
        "FROM personnel p "
        "WHERE LOWER(p.nom) LIKE :r1 OR LOWER(p.prenom) LIKE :r2 OR LOWER(p.grade) LIKE :r3 "
        "OR TO_CHAR(p.id_personnel) LIKE :r4 "
        "OR LOWER('P' || LPAD(TO_CHAR(p.id_personnel), 3, '0')) LIKE :r5 "
        + ordre;

    QString motif = "%" + recherche.trimmed().toLower() + "%";
    QSqlQuery query(DatabaseManager::database());
    query.prepare(sql);
    query.bindValue(":r1", motif);
    query.bindValue(":r2", motif);
    query.bindValue(":r3", motif);
    query.bindValue(":r4", motif);
    query.bindValue(":r5", motif);
    if (!query.exec()) {
        s_erreur = query.lastError().text();
        qDebug() << "Erreur afficher :" << s_erreur;
    }

    PersonnelModel *model = new PersonnelModel();
    model->setQuery(std::move(query));
    model->setHeaderData(0, Qt::Horizontal, QObject::tr("ID"));
    model->setHeaderData(1, Qt::Horizontal, QObject::tr("Nom"));
    model->setHeaderData(2, Qt::Horizontal, QObject::tr("Prénom"));
    model->setHeaderData(3, Qt::Horizontal, QObject::tr("Grade"));
    model->setHeaderData(4, Qt::Horizontal, QObject::tr("Poste"));
    model->setHeaderData(5, Qt::Horizontal, QObject::tr("Téléphone"));
    model->setHeaderData(6, Qt::Horizontal, QObject::tr("Disponibilité"));
    return model;
}

// Lire un membre (le mot de passe et la réponse ne sont jamais relus)
bool Personnel::trouver(int id, Personnel &resultat)
{
    QSqlQuery query(DatabaseManager::database());
    query.prepare("SELECT nom, prenom, grade, poste, telephone, email, question_secrete, disponibilite "
                  "FROM personnel WHERE id_personnel = :id");
    query.bindValue(":id", id);
    if (!query.exec() || !query.next()) {
        s_erreur = query.lastError().text();
        return false;
    }
    resultat = Personnel(id, query.value(0).toString(), query.value(1).toString(),
                         query.value(2).toString(), query.value(3).toString(),
                         query.value(4).toString(), query.value(5).toString(),
                         QString(), query.value(6).toString(), QString(),
                         query.value(7).toString());
    return true;
}

// ------------------------------------------------------------------
//  Authentification : e-mail + mot de passe (comparaison des empreintes SHA-256)
// ------------------------------------------------------------------
bool Personnel::authentifier(const QString &email, const QString &motDePasse, Personnel &connecte)
{
    QSqlQuery query(DatabaseManager::database());
    query.prepare("SELECT id_personnel, nom, prenom, grade, poste FROM personnel "
                  "WHERE LOWER(email) = :email AND mot_de_passe = :mdp");
    query.bindValue(":email", email.trimmed().toLower());
    query.bindValue(":mdp", hacher(motDePasse));
    if (!query.exec()) {
        s_erreur = query.lastError().text();
        qDebug() << "Erreur authentifier :" << s_erreur;
        return false;
    }
    if (!query.next())
        return false;
    connecte.setId(query.value(0).toInt());
    connecte.setNom(query.value(1).toString());
    connecte.setPrenom(query.value(2).toString());
    connecte.setGrade(query.value(3).toString());
    connecte.setPoste(query.value(4).toString());
    return true;
}

// ------------------------------------------------------------------
//  Mot de passe oublié : question secrète, puis réinitialisation
// ------------------------------------------------------------------
QString Personnel::questionDe(const QString &email)
{
    QSqlQuery query(DatabaseManager::database());
    query.prepare("SELECT question_secrete FROM personnel WHERE LOWER(email) = :email");
    query.bindValue(":email", email.trimmed().toLower());
    if (query.exec() && query.next())
        return query.value(0).toString();
    return QString();
}

bool Personnel::reinitialiserMotDePasse(const QString &email, const QString &reponse,
                                        const QString &nouveauMotDePasse)
{
    QSqlQuery query(DatabaseManager::database());
    query.prepare("UPDATE personnel SET mot_de_passe = :mdp "
                  "WHERE LOWER(email) = :email AND reponse_secrete = :reponse");
    query.bindValue(":mdp", hacher(nouveauMotDePasse));
    query.bindValue(":email", email.trimmed().toLower());
    query.bindValue(":reponse", hacher(normaliser(reponse)));
    if (!query.exec()) {
        s_erreur = query.lastError().text();
        qDebug() << "Erreur reinitialiser :" << s_erreur;
        return false;
    }
    return query.numRowsAffected() > 0;   // 0 ligne = mauvaise réponse
}

// ------------------------------------------------------------------
//  Statistiques : nombre de membres par grade (grades sans membre = 0)
// ------------------------------------------------------------------
QList<int> Personnel::statistiquesParGrade()
{
    const QStringList liste = grades();
    QList<int> resultat;
    for (int i = 0; i < liste.size(); i++)
        resultat << 0;

    QSqlQuery query(DatabaseManager::database());
    query.prepare("SELECT grade, COUNT(*) FROM personnel GROUP BY grade");
    if (!query.exec()) {
        s_erreur = query.lastError().text();
        qDebug() << "Erreur statistiques :" << s_erreur;
        return resultat;
    }
    while (query.next()) {
        int position = liste.indexOf(query.value(0).toString());
        if (position >= 0)
            resultat[position] = query.value(1).toInt();
    }
    return resultat;
}

// ------------------------------------------------------------------
//  Contrôle de saisie
// ------------------------------------------------------------------
bool Personnel::nomValide(const QString &texte)
{
    static const QRegularExpression re("^[\\p{L}][\\p{L} '\\-]*$",
                                       QRegularExpression::UseUnicodePropertiesOption);
    return re.match(texte.trimmed()).hasMatch();
}

bool Personnel::telephoneValide(const QString &texte)
{
    QString chiffres = texte;
    chiffres.remove(' ').remove('-');
    static const QRegularExpression re("^[0-9]{8}$");
    return re.match(chiffres).hasMatch();
}

bool Personnel::emailValide(const QString &texte)
{
    static const QRegularExpression re("^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$");
    return re.match(texte.trimmed()).hasMatch();
}

bool Personnel::motDePasseValide(const QString &texte)
{
    if (texte.length() < 6)
        return false;
    bool lettre = false, chiffre = false;
    for (const QChar &c : texte) {
        if (c.isLetter())
            lettre = true;
        else if (c.isDigit())
            chiffre = true;
    }
    return lettre && chiffre;
}

// ------------------------------------------------------------------
//  Outils
// ------------------------------------------------------------------
QString Personnel::hacher(const QString &texte)
{
    return QString::fromLatin1(QCryptographicHash::hash(texte.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QString Personnel::normaliser(const QString &reponse)
{
    return reponse.trimmed().toLower();
}

QString Personnel::derniereErreur()
{
    return s_erreur;
}

QStringList Personnel::grades()
{
    return QStringList() << "Colonel" << "Commandant" << "Capitaine" << "Lieutenant"
                         << "Adjudant" << "Sergent" << "Caporal" << "Sapeur";
}

QStringList Personnel::postes()
{
    return QStringList() << "Chef de garde" << "Intervention" << "Secours" << "Logistique" << "Administration";
}

QStringList Personnel::disponibilites()
{
    return QStringList() << "Disponible" << "En mission" << "En congé";
}

QStringList Personnel::questions()
{
    return QStringList() << "Quelle est votre ville de naissance ?"
                         << "Quel est le nom de votre premier animal ?"
                         << "Quel est le nom de votre premier professeur ?"
                         << "Quel est le nom de votre meilleur ami d'enfance ?";
}

// =====================================================================
//  3. PERSONNELMODEL (affichage du tableau)
//  Rôle Qt::UserRole : valeur brute de la cellule (sert à retrouver l'ID numérique).
// =====================================================================
QVariant PersonnelModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const int colonne = index.column();
    const QVariant brut = QSqlQueryModel::data(index, Qt::DisplayRole);

    if (role == Qt::UserRole)
        return brut;

    if (role == Qt::DisplayRole) {
        if (colonne == 0)                                   // 1 -> P001
            return QString("P%1").arg(brut.toInt(), 3, 10, QChar('0'));
        if (colonne == 5) {                                 // 22000111 -> 22 000 111
            QString tel = brut.toString();
            tel.remove(' ').remove('-');
            if (tel.length() == 8)
                return tel.left(2) + " " + tel.mid(2, 3) + " " + tel.mid(5);
        }
        return brut;
    }

    if (colonne == 6 && role == Qt::ForegroundRole) {       // disponibilité en couleur
        const QString dispo = brut.toString();
        if (dispo == "Disponible")
            return QColor("#2E7D32");
        if (dispo == "En mission")
            return QColor("#B45309");
        return QColor("#6B7280");
    }
    if (colonne == 6 && role == Qt::FontRole) {
        QFont police;
        police.setBold(true);
        return police;
    }
    return QSqlQueryModel::data(index, role);
}

// =====================================================================
//  4. PERSONNELPIECHART (diagramme circulaire avec légende)
// =====================================================================
PersonnelPieChart::PersonnelPieChart(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(235);
    setMinimumWidth(250);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void PersonnelPieChart::setData(const QStringList &labels, const QList<int> &values)
{
    m_labels = labels;
    m_values = values;
    update();
}

void PersonnelPieChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor colors[] = {QColor("#1F4E79"), QColor("#DC3C2D"), QColor("#2E7D32"), QColor("#E6A23C"),
                             QColor("#7C3AED"), QColor("#0891B2"), QColor("#64748B"), QColor("#DB2777")};
    int total = 0;
    for (int v : m_values)
        total += qMax(0, v);

    // Disposition adaptative : zone de légende réservée, cercle toujours visible en entier
    const int legendWidth = qMin(190, qMax(112, width() * 46 / 100));
    const int availablePie = qMax(90, width() - legendWidth - 24);
    const int diameter = qMax(90, qMin(170, qMin(availablePie - 8, height() - 24)));
    const int px = 8;
    const int py = qMax(8, (height() - diameter) / 2);
    QRectF pie(px, py, diameter, diameter);

    if (total <= 0) {
        p.setPen(QPen(QColor("#E5E7EB"), 18));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(pie.adjusted(10, 10, -10, -10));
        p.setPen(QColor("#64748B"));
        p.drawText(pie, Qt::AlignCenter, "Aucune donnée");
        return;
    }

    // Secteurs
    int start = 90 * 16;
    for (int i = 0; i < m_values.size(); ++i) {
        int val = qMax(0, m_values[i]);
        if (!val)
            continue;
        int span = -qRound(360.0 * 16 * val / total);
        p.setPen(QPen(Qt::white, 1.5));
        p.setBrush(colors[i % 8]);
        p.drawPie(pie, start, span);
        start += span;
    }

    // Centre blanc avec le total
    int hole = diameter / 3;
    p.setPen(Qt::white);
    p.setBrush(Qt::white);
    p.drawEllipse(pie.adjusted(hole, hole, -hole, -hole));
    p.setPen(QColor("#1F2937"));
    QFont f = p.font();
    f.setBold(true);
    f.setPointSize(17);
    p.setFont(f);
    p.drawText(QRectF(pie.x() + diameter / 4, pie.y() + diameter / 2 - 18, diameter / 2, 24),
               Qt::AlignCenter, QString::number(total));
    f.setBold(false);
    f.setPointSize(8);
    p.setFont(f);
    p.setPen(QColor("#64748B"));
    p.drawText(QRectF(pie.x() + diameter / 4, pie.y() + diameter / 2 + 5, diameter / 2, 18),
               Qt::AlignCenter, QStringLiteral("Total"));

    // Légende : un grade par ligne avec effectif et pourcentage
    const int legendX = px + diameter + 12;
    const int lineHeight = qMin(28, qMax(20, (height() - 16) / qMax(1, m_labels.size())));
    f = p.font();
    f.setPointSize(8);
    p.setFont(f);
    for (int i = 0; i < m_labels.size() && i < m_values.size(); ++i) {
        int y = 8 + i * lineHeight;
        p.setPen(Qt::NoPen);
        p.setBrush(colors[i % 8]);
        p.drawRoundedRect(QRectF(legendX, y + 5, 9, 9), 2, 2);
        double pct = total ? (100.0 * m_values[i] / total) : 0;
        p.setPen(QColor("#334155"));
        QString line = QString("%1  %2 (%3%)").arg(m_labels[i]).arg(m_values[i]).arg(qRound(pct));
        QRectF textRect(legendX + 14, y, width() - legendX - 17, lineHeight);
        p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, line);
    }
}

// =====================================================================
//  5. PERSONNELPAGE (vue : page intégrée à la fenêtre principale)
// =====================================================================
PersonnelPage::PersonnelPage(QWidget *parent)
    : QWidget(parent), m_model(nullptr), m_idEdition(0), m_tri(0)
{
    setObjectName("pagePersonnel");
    construireInterface();
    viderFormulaire();
    // Les donnees sont chargees par la fenetre principale apres la connexion
    // Oracle (appel de rafraichir() depuis GSmartFireStation::demarrer()).
}

PersonnelPage::~PersonnelPage()
{
    delete m_model;
}

// ------------------------------------------------------------------
//  Messages : relayés vers la barre d'état de la fenêtre principale
// ------------------------------------------------------------------
void PersonnelPage::showMessage(const QString &texte, int duree)
{
    if (QMainWindow *fenetre = qobject_cast<QMainWindow *>(window()))
        fenetre->statusBar()->showMessage(texte, duree);
}

void PersonnelPage::clearMessage()
{
    if (QMainWindow *fenetre = qobject_cast<QMainWindow *>(window()))
        fenetre->statusBar()->clearMessage();
}

// ------------------------------------------------------------------
//  Construction de l'interface : contenu de la page "Gestion du personnel"
//  (la barre latérale est celle, partagée, de la fenêtre principale)
// ------------------------------------------------------------------
void PersonnelPage::construireInterface()
{
    QVBoxLayout *vPage = new QVBoxLayout(this);
    vPage->setSpacing(14);
    vPage->setContentsMargins(30, 26, 30, 20);

    QLabel *titre = new QLabel("Gestion du personnel");
    titre->setProperty("class", "pageTitle");
    titre->setMinimumHeight(36);
    QHBoxLayout *entete = new QHBoxLayout();
    entete->addWidget(titre);
    entete->addStretch();
    vPage->addLayout(entete);

    QLabel *sousTitre = new QLabel("CRUD, recherche, tri, export et statistiques");
    sousTitre->setProperty("class", "pageSubtitle");
    vPage->addWidget(sousTitre);

    QFrame *separateur = new QFrame();
    separateur->setFrameShape(QFrame::HLine);
    separateur->setFrameShadow(QFrame::Plain);
    separateur->setProperty("class", "separator");
    vPage->addWidget(separateur);

    // ----- Barre d'outils : recherche, tri, export -----
    QFrame *barre = new QFrame();
    barre->setProperty("class", "card");
    barre->setFrameShape(QFrame::StyledPanel);
    barre->setFrameShadow(QFrame::Plain);
    QHBoxLayout *laBarreOutils = new QHBoxLayout(barre);
    laBarreOutils->setSpacing(12);
    laBarreOutils->setContentsMargins(18, 12, 18, 12);

    m_recherche = new QLineEdit();
    m_recherche->setPlaceholderText("Nom / ID / Grade");
    m_recherche->setClearButtonEnabled(true);
    m_recherche->setMinimumWidth(220);
    m_comboTri = new QComboBox();
    m_comboTri->setMinimumWidth(150);
    m_comboTri->addItems(QStringList() << "Grade" << "Disponibilité" << "Nom");

    QPushButton *btnRechercher = new QPushButton("Rechercher");
    btnRechercher->setProperty("class", "btnPrimary");
    QPushButton *btnTrier = new QPushButton("Trier");
    btnTrier->setProperty("class", "btnSecondary");
    QPushButton *btnExport = new QPushButton("Exporter PDF");
    btnExport->setProperty("class", "btnSecondary");
    for (QPushButton *b : {btnRechercher, btnTrier, btnExport})
        b->setCursor(Qt::PointingHandCursor);

    laBarreOutils->addWidget(new QLabel("Recherche :"));
    laBarreOutils->addWidget(m_recherche);
    laBarreOutils->addWidget(new QLabel("Trier par :"));
    laBarreOutils->addWidget(m_comboTri);
    laBarreOutils->addWidget(btnRechercher);
    laBarreOutils->addWidget(btnTrier);
    laBarreOutils->addWidget(btnExport);
    laBarreOutils->addStretch();
    vPage->addWidget(barre);

    // ----- Corps : colonne gauche (tableau, boutons, formulaire) + colonne droite (statistiques) -----
    QHBoxLayout *corps = new QHBoxLayout();
    corps->setSpacing(18);
    vPage->addLayout(corps, 1);

    QVBoxLayout *gauche = new QVBoxLayout();
    gauche->setSpacing(14);
    corps->addLayout(gauche, 1);

    // Tableau
    m_table = new QTableView();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(30);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    gauche->addWidget(m_table);

    // Boutons Ajouter / Modifier / Supprimer
    QWidget *actions = new QWidget();
    QHBoxLayout *laActions = new QHBoxLayout(actions);
    laActions->setContentsMargins(0, 8, 0, 0);
    laActions->setSpacing(10);
    QPushButton *btnAjouter = new QPushButton("Ajouter");
    btnAjouter->setProperty("class", "btnSuccess");
    QPushButton *btnModifier = new QPushButton("Modifier");
    btnModifier->setProperty("class", "btnSecondary");
    QPushButton *btnSupprimer = new QPushButton("Supprimer");
    btnSupprimer->setProperty("class", "btnDanger");
    for (QPushButton *b : {btnAjouter, btnModifier, btnSupprimer}) {
        b->setCursor(Qt::PointingHandCursor);
        laActions->addWidget(b);
    }
    laActions->addStretch(1);
    gauche->addWidget(actions);

    // Formulaire permanent : l'ajout et la modification se font directement dans la page
    m_formulaire = new QGroupBox("Fiche du personnel — remplissez les champs puis validez");
    m_formulaire->setObjectName("formPersonnelInline");
    m_formulaire->setStyleSheet("QGroupBox { border: 1px solid #D7DEE8; border-radius: 8px; margin-top: 10px; padding: 10px; font-weight: 600; } "
                                "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; color: #1F4E79; }");

    m_labelId = new QLabel();
    m_nom = new QLineEdit();
    m_nom->setPlaceholderText("Nom");
    m_prenom = new QLineEdit();
    m_prenom->setPlaceholderText("Prénom");
    m_grade = new QComboBox();
    m_grade->addItems(Personnel::grades());
    m_poste = new QComboBox();
    m_poste->addItems(Personnel::postes());
    m_telephone = new QLineEdit();
    m_telephone->setPlaceholderText("8 chiffres");
    m_email = new QLineEdit();
    m_email->setPlaceholderText("prenom.nom@firestation.tn");
    m_motDePasse = new QLineEdit();
    m_motDePasse->setEchoMode(QLineEdit::Password);
    m_motDePasse->setToolTip("6 caractères minimum, avec au moins une lettre et un chiffre");
    m_disponibilite = new QComboBox();
    m_disponibilite->addItems(Personnel::disponibilites());
    m_reponse = new QLineEdit();
    m_question = new QComboBox();
    m_question->addItems(Personnel::questions());

    QGridLayout *grille = new QGridLayout(m_formulaire);
    grille->setContentsMargins(8, 12, 8, 8);
    grille->setHorizontalSpacing(8);
    grille->setVerticalSpacing(5);
    auto ajouterChamp = [&](int ligne, int colonne, int largeur, const QString &nom, QWidget *champ) {
        QLabel *label = new QLabel(nom);
        label->setStyleSheet("font-weight: normal;");
        QVBoxLayout *cellule = new QVBoxLayout;
        cellule->setContentsMargins(0, 0, 0, 0);
        cellule->setSpacing(2);
        cellule->addWidget(label);
        cellule->addWidget(champ);
        QWidget *enveloppe = new QWidget;
        enveloppe->setLayout(cellule);
        grille->addWidget(enveloppe, ligne, colonne, 1, largeur);
    };
    // Deux lignes de champs, comme dans la maquette d'origine (6 colonnes)
    ajouterChamp(0, 0, 1, "ID", m_labelId);
    ajouterChamp(0, 1, 1, "Nom", m_nom);
    ajouterChamp(0, 2, 1, "Prénom", m_prenom);
    ajouterChamp(0, 3, 1, "Grade", m_grade);
    ajouterChamp(0, 4, 1, "Poste", m_poste);
    ajouterChamp(0, 5, 1, "Disponibilité", m_disponibilite);
    ajouterChamp(1, 0, 1, "Téléphone", m_telephone);
    ajouterChamp(1, 1, 1, "Email", m_email);
    ajouterChamp(1, 2, 1, "Mot de passe", m_motDePasse);
    ajouterChamp(1, 3, 2, "Question secrète", m_question);
    ajouterChamp(1, 5, 1, "Réponse secrète", m_reponse);
    for (int c = 0; c < 6; ++c)
        grille->setColumnStretch(c, 1);

    QPushButton *btnEnregistrer = new QPushButton("Enregistrer");
    btnEnregistrer->setObjectName("btnValiderPersonnelInline");
    btnEnregistrer->setStyleSheet("QPushButton { background:#1F4E79; color:white; border-radius:5px; padding:6px 10px; font-weight:600; }");
    QPushButton *btnEffacer = new QPushButton("Effacer");
    btnEffacer->setObjectName("btnAnnulerPersonnelInline");
    QHBoxLayout *boutonsForm = new QHBoxLayout;
    boutonsForm->addWidget(btnEnregistrer);
    boutonsForm->addWidget(btnEffacer);
    grille->addLayout(boutonsForm, 2, 0, 1, 6);
    gauche->addWidget(m_formulaire);

    // ----- Colonne droite : statistiques -----
    QVBoxLayout *droite = new QVBoxLayout();
    droite->setSpacing(16);
    corps->addLayout(droite);

    QFrame *cadreStats = new QFrame();
    cadreStats->setProperty("class", "card");
    cadreStats->setFrameShape(QFrame::StyledPanel);
    cadreStats->setFrameShadow(QFrame::Plain);
    cadreStats->setMinimumWidth(285);
    cadreStats->setMaximumWidth(340);
    cadreStats->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    QVBoxLayout *laStats = new QVBoxLayout(cadreStats);
    laStats->setSpacing(14);
    laStats->setContentsMargins(20, 18, 20, 18);

    QLabel *titreStats = new QLabel("Statistiques");
    titreStats->setProperty("class", "cardTitle");
    laStats->addWidget(titreStats);

    m_graphique = new PersonnelPieChart();
    m_graphique->setMinimumHeight(390);
    QScrollArea *defilement = new QScrollArea();
    defilement->setWidgetResizable(true);
    defilement->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    defilement->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    defilement->setFrameShape(QFrame::NoFrame);
    QWidget *contenuStats = new QWidget();
    QVBoxLayout *laContenu = new QVBoxLayout(contenuStats);
    laContenu->setContentsMargins(4, 4, 4, 4);
    laContenu->addWidget(m_graphique);
    defilement->setWidget(contenuStats);
    laStats->addWidget(defilement, 1);

    m_labelTotal = new QLabel("Total : 0 personnel(s)");
    m_labelTotal->setProperty("class", "cardDetail");
    laStats->addWidget(m_labelTotal);

    droite->addWidget(cadreStats);
    droite->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    // ----- Connexions signaux / slots -----
    connect(btnAjouter, &QPushButton::clicked, this, &PersonnelPage::ajouterPersonnel);
    connect(btnModifier, &QPushButton::clicked, this, &PersonnelPage::modifierPersonnel);
    connect(btnSupprimer, &QPushButton::clicked, this, &PersonnelPage::supprimerPersonnel);
    connect(btnEnregistrer, &QPushButton::clicked, this, &PersonnelPage::enregistrer);
    connect(btnEffacer, &QPushButton::clicked, this, &PersonnelPage::viderFormulaire);
    connect(btnRechercher, &QPushButton::clicked, this, &PersonnelPage::rechercher);
    connect(m_recherche, &QLineEdit::textChanged, this, &PersonnelPage::rechercher);   // recherche pendant la frappe
    connect(btnTrier, &QPushButton::clicked, this, &PersonnelPage::trier);
    connect(btnExport, &QPushButton::clicked, this, &PersonnelPage::exporterPdf);
    connect(m_table, &QTableView::doubleClicked, this, &PersonnelPage::modifierPersonnel);   // double-clic = modifier
}

// ------------------------------------------------------------------
//  Authentification (métier avancé)
// ------------------------------------------------------------------
bool PersonnelPage::connexion()
{
    QDialog dlg;
    dlg.setWindowTitle("Authentification");
    dlg.setFixedWidth(380);

    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(8);
    QLabel *logo = new QLabel();
    logo->setPixmap(QPixmap(":/images/logo.png").scaled(110, 110, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);
    QLabel *titre = new QLabel("Smart Fire Station");
    titre->setAlignment(Qt::AlignCenter);
    titre->setStyleSheet("font-size: 20px; font-weight: bold; color: #1F4E79;");
    QLabel *sous = new QLabel("Connexion à votre compte");
    sous->setAlignment(Qt::AlignCenter);
    sous->setProperty("class", "pageSubtitle");

    QLineEdit *email = new QLineEdit();
    email->setPlaceholderText("Email");
    QLineEdit *mdp = new QLineEdit();
    mdp->setPlaceholderText("Mot de passe");
    mdp->setEchoMode(QLineEdit::Password);
    QLabel *erreur = new QLabel();
    erreur->setProperty("class", "textDanger");
    erreur->setAlignment(Qt::AlignCenter);

    QPushButton *btnConnexion = new QPushButton("Se connecter");
    btnConnexion->setProperty("class", "btnPrimary");
    btnConnexion->setDefault(true);
    QPushButton *btnOublie = new QPushButton("Mot de passe oublié ?");
    btnOublie->setFlat(true);
    btnOublie->setCursor(Qt::PointingHandCursor);
    btnOublie->setStyleSheet("QPushButton { background: transparent; border: none; color: #1F4E79; text-decoration: underline; }");
    QPushButton *btnAnnuler = new QPushButton("Quitter");
    btnAnnuler->setProperty("class", "btnDanger");

    lay->addWidget(logo);
    lay->addWidget(titre);
    lay->addWidget(sous);
    lay->addSpacing(8);
    lay->addWidget(email);
    lay->addWidget(mdp);
    lay->addWidget(erreur);
    lay->addWidget(btnConnexion);
    lay->addWidget(btnOublie);
    lay->addWidget(btnAnnuler);

    int tentatives = 0;
    connect(btnConnexion, &QPushButton::clicked, &dlg, [&]() {
        if (email->text().trimmed().isEmpty() || mdp->text().isEmpty()) {
            erreur->setText("Saisissez votre email et votre mot de passe.");
            return;
        }
        Personnel p;
        if (Personnel::authentifier(email->text(), mdp->text(), p)) {
            m_utilisateur = p;
            dlg.accept();
            return;
        }
        tentatives++;
        mdp->clear();
        if (tentatives >= 5) {
            QMessageBox::warning(&dlg, "Authentification", "Trop de tentatives échouées.");
            dlg.reject();
            return;
        }
        erreur->setText(QString("Email ou mot de passe incorrect (%1/5).").arg(tentatives));
    });
    connect(btnOublie, &QPushButton::clicked, &dlg, [&]() { motDePasseOublie(&dlg); });
    connect(btnAnnuler, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return false;

    return true;   // la fenêtre principale lit utilisateurConnecte() et affiche le rôle
}

// ------------------------------------------------------------------
//  Mot de passe oublié (métier avancé) : question secrète
// ------------------------------------------------------------------
void PersonnelPage::motDePasseOublie(QWidget *parent)
{
    QDialog dlg(parent);
    dlg.setWindowTitle("Mot de passe oublié");
    dlg.setFixedWidth(400);

    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(8);
    QLineEdit *email = new QLineEdit();
    email->setPlaceholderText("Votre email");
    QPushButton *btnQuestion = new QPushButton("Afficher ma question secrète");
    btnQuestion->setProperty("class", "btnSecondary");
    QLabel *question = new QLabel();
    question->setWordWrap(true);
    question->setProperty("class", "textPrimaryBold");
    QLineEdit *reponse = new QLineEdit();
    reponse->setPlaceholderText("Votre réponse");
    QLineEdit *nouveau = new QLineEdit();
    nouveau->setPlaceholderText("Nouveau mot de passe");
    nouveau->setEchoMode(QLineEdit::Password);
    QLineEdit *confirmation = new QLineEdit();
    confirmation->setPlaceholderText("Confirmer le nouveau mot de passe");
    confirmation->setEchoMode(QLineEdit::Password);
    QPushButton *btnReinit = new QPushButton("Réinitialiser le mot de passe");
    btnReinit->setProperty("class", "btnPrimary");
    QLabel *message = new QLabel();
    message->setWordWrap(true);
    message->setProperty("class", "textDanger");

    reponse->setEnabled(false);
    nouveau->setEnabled(false);
    confirmation->setEnabled(false);
    btnReinit->setEnabled(false);

    lay->addWidget(new QLabel("Saisissez votre email pour retrouver votre question secrète."));
    lay->addWidget(email);
    lay->addWidget(btnQuestion);
    lay->addWidget(question);
    lay->addWidget(reponse);
    lay->addWidget(nouveau);
    lay->addWidget(confirmation);
    lay->addWidget(message);
    lay->addWidget(btnReinit);

    connect(btnQuestion, &QPushButton::clicked, &dlg, [&]() {
        QString q = Personnel::questionDe(email->text());
        if (q.isEmpty()) {
            message->setText("Aucun compte ne correspond à cet email.");
            return;
        }
        message->clear();
        question->setText(q);
        reponse->setEnabled(true);
        nouveau->setEnabled(true);
        confirmation->setEnabled(true);
        btnReinit->setEnabled(true);
    });
    connect(btnReinit, &QPushButton::clicked, &dlg, [&]() {
        if (reponse->text().trimmed().isEmpty()) {
            message->setText("Saisissez votre réponse secrète.");
            return;
        }
        if (!Personnel::motDePasseValide(nouveau->text())) {
            message->setText("Mot de passe : 6 caractères minimum, avec au moins une lettre et un chiffre.");
            return;
        }
        if (nouveau->text() != confirmation->text()) {
            message->setText("Les deux mots de passe sont différents.");
            return;
        }
        if (Personnel::reinitialiserMotDePasse(email->text(), reponse->text(), nouveau->text())) {
            QMessageBox::information(&dlg, "Mot de passe oublié",
                                     "Mot de passe réinitialisé. Vous pouvez vous connecter.");
            dlg.accept();
        } else {
            message->setText("Réponse incorrecte.");
        }
    });

    dlg.exec();
}

// ------------------------------------------------------------------
//  Contrôle de saisie : remplit un objet Personnel à partir du formulaire
// ------------------------------------------------------------------
bool PersonnelPage::lireFormulaire(Personnel &p, bool modification)
{
    QString erreur;
    if (!Personnel::nomValide(m_nom->text()))
        erreur = "Nom invalide (lettres uniquement).";
    else if (!Personnel::nomValide(m_prenom->text()))
        erreur = "Prénom invalide (lettres uniquement).";
    else if (!Personnel::telephoneValide(m_telephone->text()))
        erreur = "Le téléphone doit contenir exactement 8 chiffres.";
    else if (!Personnel::emailValide(m_email->text()))
        erreur = "L'adresse email n'est pas valide.";
    else if (!modification && !Personnel::motDePasseValide(m_motDePasse->text()))
        erreur = "Mot de passe : 6 caractères minimum, avec au moins une lettre et un chiffre.";
    else if (modification && !m_motDePasse->text().isEmpty() && !Personnel::motDePasseValide(m_motDePasse->text()))
        erreur = "Mot de passe : 6 caractères minimum, avec au moins une lettre et un chiffre.";
    else if (!modification && m_reponse->text().trimmed().length() < 2)
        erreur = "Réponse secrète obligatoire (2 caractères minimum).";

    if (!erreur.isEmpty()) {
        QMessageBox::warning(this, "Saisie incorrecte", erreur);
        return false;
    }

    QString tel = m_telephone->text();
    tel.remove(' ').remove('-');

    p.setNom(m_nom->text().trimmed());
    p.setPrenom(m_prenom->text().trimmed());
    p.setGrade(m_grade->currentText());
    p.setPoste(m_poste->currentText());
    p.setTelephone(tel);
    p.setEmail(m_email->text().trimmed());
    p.setMotDePasse(m_motDePasse->text());
    p.setQuestionSecrete(m_question->currentText());
    p.setReponseSecrete(m_reponse->text());
    p.setDisponibilite(m_disponibilite->currentText());
    return true;
}

// ------------------------------------------------------------------
//  CRUD : la vue appelle uniquement les méthodes de la classe Personnel
// ------------------------------------------------------------------

// Bouton "Ajouter" : le formulaire est vidé, puis "Enregistrer" crée le membre
void PersonnelPage::ajouterPersonnel()
{
    viderFormulaire();
    m_formulaire->setTitle("Ajouter un personnel — remplissez la fiche ci-dessous");
    m_nom->setFocus();
    showMessage("Remplissez le formulaire visible puis cliquez sur Enregistrer.", 5000);
}

// Bouton "Modifier" (ou double-clic) : on relit le membre en base et on remplit le formulaire
void PersonnelPage::modifierPersonnel()
{
    int id = idSelectionne();
    if (id == 0) {
        QMessageBox::information(this, "Modifier un personnel", "Sélectionnez d'abord un personnel dans la liste.");
        return;
    }
    Personnel p;
    if (!Personnel::trouver(id, p)) {
        QMessageBox::critical(this, "Base de données", "Impossible de lire ce membre du personnel.");
        return;
    }

    m_idEdition = id;
    m_labelId->setText(QString("P%1").arg(id, 3, 10, QChar('0')));
    m_nom->setText(p.nom());
    m_prenom->setText(p.prenom());
    m_telephone->setText(p.telephone());
    m_email->setText(p.email());
    m_grade->setCurrentText(p.grade());
    m_poste->setCurrentText(p.poste());
    m_question->setCurrentText(p.questionSecrete());
    m_disponibilite->setCurrentText(p.disponibilite());
    m_motDePasse->clear();
    m_motDePasse->setPlaceholderText("Laisser vide = inchangé");
    m_reponse->clear();
    m_reponse->setPlaceholderText("Laisser vide = inchangée");
    m_formulaire->setTitle(QString("Modifier P%1 — modifiez les champs puis enregistrez")
                               .arg(id, 3, 10, QChar('0')));
    m_nom->setFocus();
}

// Bouton "Enregistrer" : INSERT (nouveau membre) ou UPDATE (membre en modification)
void PersonnelPage::enregistrer()
{
    bool modification = (m_idEdition != 0);
    Personnel p;
    if (!lireFormulaire(p, modification))
        return;

    if (modification) {
        p.setId(m_idEdition);
        if (!p.modifier()) {
            QMessageBox::critical(this, "Base de données",
                                  "La modification a échoué (cet email est peut-être déjà utilisé).");
            return;
        }
        showMessage("Personnel modifié avec succès.", 4000);
    } else {
        if (!p.ajouter()) {
            QMessageBox::critical(this, "Base de données",
                                  "L'ajout a échoué (cet email est peut-être déjà utilisé).");
            return;
        }
        showMessage("Personnel ajouté avec succès.", 4000);
    }
    viderFormulaire();
    rafraichir();
}

void PersonnelPage::supprimerPersonnel()
{
    int id = idSelectionne();
    if (id == 0) {
        QMessageBox::information(this, "Supprimer un personnel", "Sélectionnez d'abord un personnel dans la liste.");
        return;
    }
    if (id == m_utilisateur.id()) {
        QMessageBox::warning(this, "Supprimer un personnel", "Vous ne pouvez pas supprimer le compte connecté.");
        return;
    }
    Personnel p;
    Personnel::trouver(id, p);
    if (QMessageBox::question(this, "Supprimer un personnel",
                              "Voulez-vous vraiment supprimer " + p.nomComplet() + " ?") != QMessageBox::Yes)
        return;
    if (!Personnel::supprimer(id)) {
        QMessageBox::critical(this, "Base de données", "La suppression a échoué.");
        return;
    }
    showMessage("Personnel supprimé.", 5000);
    viderFormulaire();
    rafraichir();
}

// Bouton "Effacer" : le formulaire revient à l'état "nouveau membre"
void PersonnelPage::viderFormulaire()
{
    m_idEdition = 0;
    m_labelId->setText("(automatique)");
    m_nom->clear();
    m_prenom->clear();
    m_telephone->clear();
    m_email->clear();
    m_motDePasse->clear();
    m_motDePasse->setPlaceholderText("6 caractères min.");
    m_reponse->clear();
    m_reponse->setPlaceholderText("Réponse");
    m_grade->setCurrentIndex(0);
    m_poste->setCurrentIndex(0);
    m_question->setCurrentIndex(0);
    m_disponibilite->setCurrentText("Disponible");
    m_formulaire->setTitle("Fiche du personnel — remplissez les champs puis validez");
    m_table->clearSelection();
}

// ------------------------------------------------------------------
//  Recherche, tri, affichage : exécutés par la classe Personnel (SQL)
// ------------------------------------------------------------------
void PersonnelPage::rechercher()
{
    rafraichir();
    QString texte = m_recherche->text().trimmed();
    if (texte.isEmpty())
        clearMessage();
    else
        showMessage(QString("%1 résultat(s)").arg(m_model->rowCount()), 4000);
}

void PersonnelPage::trier()
{
    m_tri = m_comboTri->currentIndex() + 1;   // 0 Grade -> 1, 1 Disponibilité -> 2, 2 Nom -> 3
    rafraichir();
    showMessage("Liste triée.", 4000);
}

void PersonnelPage::rafraichir()
{
    QSqlQueryModel *nouveau = Personnel::afficher(m_recherche->text(), m_tri);
    while (nouveau->canFetchMore())
        nouveau->fetchMore();
    m_table->setModel(nouveau);
    delete m_model;
    m_model = nouveau;

    // Colonnes : ID étroite, les autres se partagent la place
    QHeaderView *entete = m_table->horizontalHeader();
    entete->setSectionResizeMode(QHeaderView::Stretch);
    entete->setSectionResizeMode(0, QHeaderView::Fixed);
    entete->resizeSection(0, 70);

    mettreAJourStatistiques();
}

// ID du membre sélectionné dans le tableau (0 si aucune ligne n'est sélectionnée)
int PersonnelPage::idSelectionne() const
{
    QModelIndex ligne = m_table->currentIndex();
    if (!ligne.isValid() || !m_table->selectionModel()->isRowSelected(ligne.row(), QModelIndex()))
        return 0;
    return m_model->data(m_model->index(ligne.row(), 0), Qt::UserRole).toInt();
}

// Panneau "Statistiques" : diagramme par grade + total
void PersonnelPage::mettreAJourStatistiques()
{
    QList<int> valeurs = Personnel::statistiquesParGrade();
    int total = 0;
    for (int v : valeurs)
        total += v;
    m_graphique->setData(Personnel::grades(), valeurs);
    m_labelTotal->setText(QString("Total : %1 personnel(s)").arg(total));
}

// ------------------------------------------------------------------
//  Export PDF : les lignes affichées (donc le résultat d'une recherche / d'un tri)
// ------------------------------------------------------------------
void PersonnelPage::exporterPdf()
{
    const int colonnes = m_model->columnCount();
    const int lignes = m_model->rowCount();
    if (lignes == 0) {
        QMessageBox::information(this, "Export PDF", "Aucune donnée à exporter.");
        return;
    }

    QString dossier = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString chemin = QFileDialog::getSaveFileName(this, "Exporter en PDF",
                                                  dossier + "/personnel.pdf", "Fichiers PDF (*.pdf)");
    if (chemin.isEmpty())
        return;   // l'utilisateur a annulé
    if (!chemin.endsWith(".pdf"))
        chemin += ".pdf";

    // Le contenu du PDF est écrit en HTML (titre + tableau), puis "imprimé" dans le fichier
    QString html = "<h2 style='color:#1F4E79;'>Smart Fire Station Management</h2>"
                   "<h3>Liste du personnel</h3>"
                   "<p>Exporté le " + QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm") + "</p>"
                   "<table border='1' cellspacing='0' cellpadding='5' width='100%'><tr>";
    for (int c = 0; c < colonnes; c++)
        html += "<th bgcolor='#1F4E79' style='color:white;'>"
                + m_model->headerData(c, Qt::Horizontal).toString().toHtmlEscaped() + "</th>";
    html += "</tr>";
    for (int l = 0; l < lignes; l++) {
        html += "<tr>";
        for (int c = 0; c < colonnes; c++)
            html += "<td>" + m_model->data(m_model->index(l, c)).toString().toHtmlEscaped() + "</td>";
        html += "</tr>";
    }
    html += "</table>";

    QPdfWriter writer(chemin);
    QTextDocument document;
    document.setHtml(html);
    document.print(&writer);

    showMessage("PDF exporté : " + chemin, 6000);
    if (QMessageBox::question(this, "Export PDF",
                              "Le PDF a été créé.\nVoulez-vous l'ouvrir maintenant ?") == QMessageBox::Yes)
        QDesktopServices::openUrl(QUrl::fromLocalFile(chemin));
}

