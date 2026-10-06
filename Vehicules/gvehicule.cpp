#include "gvehicule.h"
#include "ui_gvehicule.h"

#include "pdfexport.h"
#include "statsdialog.h"
#include "styleutils.h"
#include "vehiculedialog.h"

#include <QDesktopServices>
#include <QFileDialog>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTableWidget>
#include <QUrl>

GVehicule::GVehicule(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::GVehicule)
{
    ui->setupUi(this);
    repolish(this); // applique les class="..." du .ui au fichier style.qss
    setWindowIcon(QIcon(":/images/icon.png"));

    setupVehicules();
    chargerVehiculesExemple();
    refreshVehiculesTable();
}

GVehicule::~GVehicule()
{
    delete ui;
}

// Bouton Quitter
void GVehicule::on_btnQuitterVehicule_clicked()
{
    int reponse = QMessageBox::question(this, "Quitter", "Voulez-vous vraiment quitter l'application ?");
    if (reponse == QMessageBox::Yes)
        close();
}

// ===================== OUTILS COMMUNS =====================

// Crée une case de tableau avec une couleur de texte (et éventuellement en gras)
QTableWidgetItem *GVehicule::creerItem(const QString &texte, const QColor &couleur, bool gras)
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
void GVehicule::setLabelClass(QLabel *label, const QString &classe)
{
    label->setProperty("class", classe);
    label->style()->unpolish(label);
    label->style()->polish(label);
}

// Exporte en PDF les lignes visibles d'un tableau (donc le résultat d'une recherche / d'un tri)
void GVehicule::exporterTableauPdf(QTableWidget *table, const QString &titre, const QString &nomFichier)
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

// =====================================================================
//                        MODULE VÉHICULES
// =====================================================================

void GVehicule::setupVehicules()
{
    m_vehiculePropose = -1;

    // Colonnes du tableau : ID étroite, les autres se partagent la place
    QHeaderView *entete = ui->tableWidgetVehicules->horizontalHeader();
    entete->setSectionResizeMode(QHeaderView::Stretch);
    entete->setSectionResizeMode(0, QHeaderView::Fixed);
    entete->resizeSection(0, 70);
    entete->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

// Données de démonstration (à remplacer plus tard par la base de données)
void GVehicule::chargerVehiculesExemple()
{
    m_vehicules.clear();
    m_vehicules.append(Vehicule("V001", "123 TU 456", "Fourgon", "Renault", "Bon", "Disponible", QDate(2026, 11, 10)));
    m_vehicules.append(Vehicule("V002", "124 TU 457", "Camion", "Iveco", "Bon", "En mission", QDate(2026, 10, 22)));
    m_vehicules.append(Vehicule("V003", "125 TU 458", "Ambulance", "Mercedes", "À contrôler", "Disponible", QDate(2026, 10, 2)));
    m_vehicules.append(Vehicule("V004", "126 TU 459", "Citerne", "MAN", "Bon", "Disponible", QDate(2026, 12, 18)));
    m_vehicules.append(Vehicule("V005", "127 TU 460", "Ambulance", "Renault", "Bon", "Disponible", QDate(2027, 1, 15)));
    m_vehicules.append(Vehicule("V006", "128 TU 461", "Échelle", "Iveco", "À contrôler", "En maintenance", QDate(2026, 10, 29)));
    m_vehicules.append(Vehicule("V007", "129 TU 462", "Fourgon", "MAN", "Bon", "En maintenance", QDate(2026, 10, 12)));
}

// Numéro de la ligne sélectionnée dans le tableau (-1 si aucune)
int GVehicule::vehiculeSelectionne()
{
    int ligne = ui->tableWidgetVehicules->currentRow();
    if (ligne < 0 || ligne >= m_vehicules.size())
        return -1;
    return ligne;
}

// Prochain ID libre : V001, V002, ...
QString GVehicule::prochainIdVehicule()
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
void GVehicule::refreshVehiculesTable()
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
    ui->progressStatVehDispo->setMaximum(maximum);
    ui->progressStatVehDispo->setValue(dispo);
    ui->labelStatVehMaint->setText(QString("En maintenance : %1").arg(maintenance));

    refreshVehiculePropose();
}

// ------------------------------------------------------------------ CRUD
void GVehicule::on_btnAjouterVehicule_clicked()
{
    QStringList immatriculations;
    for (int i = 0; i < m_vehicules.size(); i++)
        immatriculations << m_vehicules[i].immatriculation();

    VehiculeDialog formulaire("Ajouter un véhicule", prochainIdVehicule(), immatriculations, this);
    if (formulaire.exec() != QDialog::Accepted)
        return; // annulé

    m_vehicules.append(formulaire.vehicule());
    refreshVehiculesTable();
    ui->tableWidgetVehicules->selectRow(m_vehicules.size() - 1);
    statusBar()->showMessage("Véhicule ajouté.", 5000);
}

void GVehicule::on_btnModifierVehicule_clicked()
{
    int i = vehiculeSelectionne();
    if (i < 0) {
        QMessageBox::information(this, "Modifier un véhicule",
                                 "Sélectionnez d'abord un véhicule dans la liste.");
        return;
    }
    // Immatriculations des AUTRES véhicules (celle du véhicule modifié reste autorisée)
    QStringList immatriculations;
    for (int k = 0; k < m_vehicules.size(); k++) {
        if (k != i)
            immatriculations << m_vehicules[k].immatriculation();
    }
    VehiculeDialog formulaire("Modifier un véhicule", m_vehicules[i].id(), immatriculations, this);
    formulaire.setVehicule(m_vehicules[i]);
    if (formulaire.exec() != QDialog::Accepted)
        return;

    m_vehicules[i] = formulaire.vehicule();
    refreshVehiculesTable();
    statusBar()->showMessage("Véhicule modifié.", 5000);
}

void GVehicule::on_btnSupprimerVehicule_clicked()
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

    m_vehicules.removeAt(i);
    ui->tableWidgetVehicules->setCurrentCell(-1, -1);
    refreshVehiculesTable();
    statusBar()->showMessage("Véhicule supprimé.", 5000);
}

void GVehicule::on_tableWidgetVehicules_cellDoubleClicked(int, int)
{
    on_btnModifierVehicule_clicked(); // double-clic = modifier
}

// ------------------------------------------------------------------ recherche
// Cache les lignes qui ne contiennent pas le texte cherché (immatriculation, type, modèle ou ID)
void GVehicule::filtrerVehicules()
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

void GVehicule::on_btnRechercherVehicule_clicked()
{
    filtrerVehicules();
}

void GVehicule::on_lineEditRechercheVehicule_textChanged(const QString &)
{
    filtrerVehicules(); // la recherche se met à jour pendant la frappe
}

// ------------------------------------------------------------------ tri (tri à bulles)
// Retourne vrai si 'a' doit passer après 'b' selon le critère choisi
bool GVehicule::doitEchangerVehicules(Vehicule a, Vehicule b, int critere)
{
    if (critere == 0) // État : Bon, À contrôler, Hors service
        return Vehicule::etats().indexOf(a.etat()) > Vehicule::etats().indexOf(b.etat());
    // Type : Fourgon, Camion, Ambulance, Citerne, Échelle
    return Vehicule::types().indexOf(a.type()) > Vehicule::types().indexOf(b.type());
}

void GVehicule::on_btnTrierVehicule_clicked()
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
void GVehicule::on_btnExportVehicule_clicked()
{
    exporterTableauPdf(ui->tableWidgetVehicules, "Liste des véhicules", "vehicules.pdf");
}

// ------------------------------------------------------------------ statistiques
StatCritere GVehicule::statVehiculesParEtat()
{
    QStringList reelles;
    for (int i = 0; i < m_vehicules.size(); i++)
        reelles << m_vehicules[i].etat();
    return compterParValeur("État", Vehicule::etats(), reelles);
}

StatCritere GVehicule::statVehiculesParDispo()
{
    QStringList reelles;
    for (int i = 0; i < m_vehicules.size(); i++)
        reelles << m_vehicules[i].disponibilite();
    return compterParValeur("Disponibilité", Vehicule::disponibilites(), reelles);
}

void GVehicule::on_btnStatsVehicule_clicked()
{
    QList<StatCritere> criteres;
    criteres << statVehiculesParEtat() << statVehiculesParDispo();
    StatsDialog fenetre("Statistiques des véhicules", criteres, this);
    fenetre.exec();
}

// ------------------------------------------------------------------ métier : choix adapté à la mission
// Propose un véhicule DISPONIBLE, en BON ÉTAT, dont le type correspond à l'intervention.
// On teste les types par ordre de préférence ; parmi les candidats on prend celui dont
// la prochaine maintenance est la plus lointaine.
void GVehicule::refreshVehiculePropose()
{
    m_vehiculePropose = -1;
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

void GVehicule::on_comboIntervention_currentIndexChanged(int)
{
    refreshVehiculePropose();
}

void GVehicule::on_btnUtiliserVehicule_clicked()
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
