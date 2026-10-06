#include "gpersonnel.h"
// Module Gestion du personnel : ce fichier relie l'interface Qt aux opérations métier.
// Les commentaires indiquent le rôle des principales étapes pour faciliter la compréhension du projet.

#include "ui_gpersonnel.h"

#include "pdfexport.h"
#include "statsdialog.h"
#include "styleutils.h"
#include "personneldialog.h"

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

GPersonnel::GPersonnel(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::GPersonnel)
{
    ui->setupUi(this);
    repolish(this); // applique les class="..." du .ui au fichier style.qss
    setWindowIcon(QIcon(":/images/icon.png"));

    setupPersonnel();
    chargerPersonnelExemple();
    refreshPersonnelTable();
}

GPersonnel::~GPersonnel()
{
    delete ui;
}

// Bouton Quitter
void GPersonnel::on_btnQuitterPersonnel_clicked()
{
    int reponse = QMessageBox::question(this, "Quitter", "Voulez-vous vraiment quitter l'application ?");
    if (reponse == QMessageBox::Yes)
        close();
}

// ===================== OUTILS COMMUNS =====================

// Crée une case de tableau avec une couleur de texte (et éventuellement en gras)
QTableWidgetItem *GPersonnel::creerItem(const QString &texte, const QColor &couleur, bool gras)
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
void GPersonnel::setLabelClass(QLabel *label, const QString &classe)
{
    label->setProperty("class", classe);
    label->style()->unpolish(label);
    label->style()->polish(label);
}

// Exporte en PDF les lignes visibles d'un tableau (donc le résultat d'une recherche / d'un tri)
void GPersonnel::exporterTableauPdf(QTableWidget *table, const QString &titre, const QString &nomFichier)
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
//                        MODULE PERSONNELS
// =====================================================================

void GPersonnel::setupPersonnel()
{
    // Colonnes du tableau : ID étroite, les autres se partagent la place
    QHeaderView *entete = ui->tableWidgetPersonnel->horizontalHeader();
    entete->setSectionResizeMode(QHeaderView::Stretch);
    entete->setSectionResizeMode(0, QHeaderView::Fixed);
    entete->resizeSection(0, 70);
    entete->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

// Données de démonstration (à remplacer plus tard par la base de données)
void GPersonnel::chargerPersonnelExemple()
{
    m_personnel.clear();
    m_personnel.append(Personnel("P001", "Ben Ali", "Ahmed", "Capitaine", "Chef de garde", "22 000 111",
                              "ahmed.benali@firestation.tn", "pass1234", "Disponible", 1));
    m_personnel.append(Personnel("P002", "Trabelsi", "Yassine", "Sergent", "Intervention", "22 000 222",
                              "yassine.trabelsi@firestation.tn", "pass1234", "En mission", 3));
    m_personnel.append(Personnel("P003", "Jlassi", "Sami", "Caporal", "Secours", "22 000 333",
                              "sami.jlassi@firestation.tn", "pass1234", "Disponible", 0));
    m_personnel.append(Personnel("P004", "Gharbi", "Mehdi", "Lieutenant", "Intervention", "22 000 555",
                              "mehdi.gharbi@firestation.tn", "pass1234", "Disponible", 1));
    m_personnel.append(Personnel("P005", "Khelifi", "Walid", "Sapeur", "Secours", "22 000 666",
                              "walid.khelifi@firestation.tn", "pass1234", "Disponible", 2));
    m_personnel.append(Personnel("P006", "Ben Salah", "Karim", "Adjudant", "Logistique", "22 000 777",
                              "karim.bensalah@firestation.tn", "pass1234", "En congé", 0));
    m_personnel.append(Personnel("P007", "Dridi", "Nour", "Sapeur", "Intervention", "22 000 888",
                              "nour.dridi@firestation.tn", "pass1234", "En mission", 4));
    m_personnel.append(Personnel("P014", "Mansour", "Omar", "Sergent", "Intervention", "22 000 444",
                              "omar.mansour@firestation.tn", "pass1234", "En mission", 5));
}

// Numéro de la ligne sélectionnée dans le tableau (-1 si aucune).
// La ligne n du tableau correspond toujours au personnel n de la liste m_personnel.
int GPersonnel::personnelSelectionne()
{
    int ligne = ui->tableWidgetPersonnel->currentRow();
    if (ligne < 0 || ligne >= m_personnel.size())
        return -1;
    return ligne;
}

// Prochain ID libre : P001, P002, ...
QString GPersonnel::prochainIdPersonnel()
{
    int max = 0;
    for (int i = 0; i < m_personnel.size(); i++) {
        int numero = m_personnel[i].id().mid(1).toInt();
        if (numero > max)
            max = numero;
    }
    return QString("P%1").arg(max + 1, 3, 10, QChar('0'));
}

// ------------------------------------------------------------------ affichage
void GPersonnel::refreshPersonnelTable()
{
    int seuil = ui->spinSeuil->value();
    int ligneCourante = ui->tableWidgetPersonnel->currentRow();
    QColor noir("#1F1F1F");

    ui->tableWidgetPersonnel->setRowCount(m_personnel.size());
    for (int r = 0; r < m_personnel.size(); r++) {
        Personnel p = m_personnel[r];

        // Nom et prénom en rouge gras si le personnel est en surcharge
        QColor couleurNom = noir;
        bool gras = false;
        if (p.charge() >= seuil) {
            couleurNom = QColor("#C00000");
            gras = true;
        }

        // Couleur de la disponibilité
        QColor couleurDispo("#6B7280");
        if (p.disponibilite() == "Disponible")
            couleurDispo = QColor("#2E7D32");
        else if (p.disponibilite() == "En mission")
            couleurDispo = QColor("#B45309");

        ui->tableWidgetPersonnel->setItem(r, 0, creerItem(p.id(), noir, false));
        ui->tableWidgetPersonnel->setItem(r, 1, creerItem(p.nom(), couleurNom, gras));
        ui->tableWidgetPersonnel->setItem(r, 2, creerItem(p.prenom(), couleurNom, gras));
        ui->tableWidgetPersonnel->setItem(r, 3, creerItem(p.grade(), noir, false));
        ui->tableWidgetPersonnel->setItem(r, 4, creerItem(p.poste(), noir, false));
        ui->tableWidgetPersonnel->setItem(r, 5, creerItem(p.telephone(), noir, false));
        ui->tableWidgetPersonnel->setItem(r, 6, creerItem(p.disponibilite(), couleurDispo, true));
    }
    if (ligneCourante >= 0 && ligneCourante < m_personnel.size())
        ui->tableWidgetPersonnel->selectRow(ligneCourante);

    filtrerPersonnel();

    // Panneau "Statistiques"
    int dispo = 0;
    int mission = 0;
    for (int i = 0; i < m_personnel.size(); i++) {
        if (m_personnel[i].disponibilite() == "Disponible")
            dispo++;
        else if (m_personnel[i].disponibilite() == "En mission")
            mission++;
    }
    int total = m_personnel.size();
    int maximum = total;
    if (maximum < 1)
        maximum = 1;
    ui->progressStatDispo->setMaximum(maximum);
    ui->progressStatDispo->setValue(dispo);
    ui->progressStatMission->setMaximum(maximum);
    ui->progressStatMission->setValue(mission);
    ui->labelTotalPersonnel->setText(QString("Total : %1 personnel(s)").arg(total));

    refreshSurcharge();
}

// Métier innovant : détection de surcharge
void GPersonnel::refreshSurcharge()
{
    int seuil = ui->spinSeuil->value();

    int plusCharge = -1;  // numéro du personnel le plus chargé (parmi ceux en surcharge)
    int nbSurcharge = 0;
    int chargeMax = 0;
    for (int i = 0; i < m_personnel.size(); i++) {
        int c = m_personnel[i].charge();
        if (c > chargeMax)
            chargeMax = c;
        if (c >= seuil) {
            nbSurcharge++;
            if (plusCharge < 0 || c > m_personnel[plusCharge].charge())
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

    Personnel p = m_personnel[plusCharge];
    ui->labelSurchargeNom->setText(p.id() + " — " + p.nomComplet());
    ui->labelSurchargeCharge->setText(QString("Charge actuelle : %1 %").arg(p.charge()));
    setLabelClass(ui->labelSurchargeCharge, "textDanger");
    QString etat = "⚠ Surcharge détectée";
    if (nbSurcharge > 1)
        etat += QString(" (+%1 autre(s))").arg(nbSurcharge - 1);
    ui->labelSurchargeEtat->setText(etat);
    setLabelClass(ui->labelSurchargeEtat, "textDangerBold");
    ui->btnDetailsSurcharge->setEnabled(true);
}

// ------------------------------------------------------------------ CRUD
void GPersonnel::on_btnAjouterPersonnel_clicked()
{
    PersonnelDialog formulaire("Ajouter un personnel", prochainIdPersonnel(), this);
    if (formulaire.exec() != QDialog::Accepted)
        return; // annulé

    m_personnel.append(formulaire.personnel());
    refreshPersonnelTable();
    ui->tableWidgetPersonnel->selectRow(m_personnel.size() - 1);
    statusBar()->showMessage("Personnel ajouté.", 5000);
}

void GPersonnel::on_btnModifierPersonnel_clicked()
{
    int i = personnelSelectionne();
    if (i < 0) {
        QMessageBox::information(this, "Modifier un personnel",
                                 "Sélectionnez d'abord un personnel dans la liste.");
        return;
    }
    PersonnelDialog formulaire("Modifier un personnel", m_personnel[i].id(), this);
    formulaire.setPersonnel(m_personnel[i]);
    if (formulaire.exec() != QDialog::Accepted)
        return;

    m_personnel[i] = formulaire.personnel();
    refreshPersonnelTable();
    statusBar()->showMessage("Personnel modifié.", 5000);
}

void GPersonnel::on_btnSupprimerPersonnel_clicked()
{
    int i = personnelSelectionne();
    if (i < 0) {
        QMessageBox::information(this, "Supprimer un personnel",
                                 "Sélectionnez d'abord un personnel dans la liste.");
        return;
    }
    int reponse = QMessageBox::question(this, "Supprimer un personnel",
                                        "Voulez-vous vraiment supprimer " + m_personnel[i].nomComplet() + " ?");
    if (reponse != QMessageBox::Yes)
        return;

    m_personnel.removeAt(i);
    ui->tableWidgetPersonnel->setCurrentCell(-1, -1);
    refreshPersonnelTable();
    statusBar()->showMessage("Personnel supprimé.", 5000);
}

void GPersonnel::on_tableWidgetPersonnel_cellDoubleClicked(int, int)
{
    on_btnModifierPersonnel_clicked(); // double-clic = modifier
}

// ------------------------------------------------------------------ recherche
// Cache les lignes qui ne contiennent pas le texte cherché (ID, nom, prénom ou grade)
void GPersonnel::filtrerPersonnel()
{
    QString texte = ui->lineEditRecherchePersonnel->text().trimmed();
    int visibles = 0;
    for (int r = 0; r < m_personnel.size(); r++) {
        Personnel p = m_personnel[r];
        bool trouve = texte == ""
                      || p.id().contains(texte, Qt::CaseInsensitive)
                      || p.nom().contains(texte, Qt::CaseInsensitive)
                      || p.prenom().contains(texte, Qt::CaseInsensitive)
                      || p.grade().contains(texte, Qt::CaseInsensitive);
        ui->tableWidgetPersonnel->setRowHidden(r, !trouve);
        if (trouve)
            visibles++;
    }
    if (texte != "")
        statusBar()->showMessage(QString("%1 résultat(s)").arg(visibles), 4000);
    else
        statusBar()->clearMessage();
}

void GPersonnel::on_btnRechercherPersonnel_clicked()
{
    filtrerPersonnel();
}

void GPersonnel::on_lineEditRecherchePersonnel_textChanged(const QString &)
{
    filtrerPersonnel(); // la recherche se met à jour pendant la frappe
}

// ------------------------------------------------------------------ tri (tri à bulles)
// Retourne vrai si 'a' doit passer après 'b' selon le critère choisi
bool GPersonnel::doitEchangerPersonnel(Personnel a, Personnel b, int critere)
{
    if (critere == 0) // Grade : du plus élevé au moins élevé
        return Personnel::rangGrade(a.grade()) > Personnel::rangGrade(b.grade());
    if (critere == 1) // Disponibilité : Disponible, En mission, En congé
        return Personnel::disponibilites().indexOf(a.disponibilite())
               > Personnel::disponibilites().indexOf(b.disponibilite());
    return a.nom().toLower() > b.nom().toLower(); // Nom : ordre alphabétique
}

void GPersonnel::on_btnTrierPersonnel_clicked()
{
    int critere = ui->comboTriPersonnel->currentIndex();
    int n = m_personnel.size();
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            if (doitEchangerPersonnel(m_personnel[j], m_personnel[j + 1], critere)) {
                Personnel temp = m_personnel[j];
                m_personnel[j] = m_personnel[j + 1];
                m_personnel[j + 1] = temp;
            }
        }
    }
    ui->tableWidgetPersonnel->setCurrentCell(-1, -1);
    refreshPersonnelTable();
    statusBar()->showMessage("Liste triée.", 4000);
}

// ------------------------------------------------------------------ export PDF
void GPersonnel::on_btnExportPersonnel_clicked()
{
    exporterTableauPdf(ui->tableWidgetPersonnel, "Liste du personnel", "personnel.pdf");
}

// ------------------------------------------------------------------ statistiques
StatCritere GPersonnel::statPersonnelParGrade()
{
    QStringList reelles;
    for (int i = 0; i < m_personnel.size(); i++)
        reelles << m_personnel[i].grade();
    return compterParValeur("Grade", Personnel::grades(), reelles);
}

StatCritere GPersonnel::statPersonnelParDispo()
{
    QStringList reelles;
    for (int i = 0; i < m_personnel.size(); i++)
        reelles << m_personnel[i].disponibilite();
    return compterParValeur("Disponibilité", Personnel::disponibilites(), reelles);
}

void GPersonnel::on_btnStatsPersonnel_clicked()
{
    QList<StatCritere> criteres;
    criteres << statPersonnelParGrade() << statPersonnelParDispo();
    StatsDialog fenetre("Statistiques du personnel", criteres, this);
    fenetre.exec();
}

// ------------------------------------------------------------------ surcharge
void GPersonnel::on_spinSeuil_valueChanged(int)
{
    refreshPersonnelTable(); // le seuil a changé : on recalcule les surcharges
}

void GPersonnel::on_btnDetailsSurcharge_clicked()
{
    int seuil = ui->spinSeuil->value();
    QString texte = "";
    int nb = 0;
    int plusCharge = -1;
    for (int i = 0; i < m_personnel.size(); i++) {
        Personnel p = m_personnel[i];
        if (p.charge() >= seuil) {
            nb++;
            texte += QString("• %1 — %2 (%3) : %4 mission(s), charge %5 %\n")
                         .arg(p.id()).arg(p.nomComplet()).arg(p.grade())
                         .arg(p.missionsEnCours()).arg(p.charge());
            if (plusCharge < 0 || p.charge() > m_personnel[plusCharge].charge())
                plusCharge = i;
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

    ui->tableWidgetPersonnel->selectRow(plusCharge); // montre le plus chargé dans le tableau
}
