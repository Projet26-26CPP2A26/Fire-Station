#ifndef GPERSONNEL_H
#define GPERSONNEL_H

#include <QColor>
#include <QList>
#include <QMainWindow>
// Fenêtre principale du module Personnel : elle orchestre le CRUD, la recherche, le tri, l'export et les statistiques.


#include "personnel.h"
#include "statsdialog.h"
#include "vehicule.h"

class QLabel;
class QTableWidget;
class QTableWidgetItem;

QT_BEGIN_NAMESPACE
namespace Ui {
class GPersonnel;
}
QT_END_NAMESPACE

// Projet individuel : module "Gestion du personnel".
// Les noms des objets et des slots sont les mêmes que dans le projet d'intégration
// SmartFireStation : l'intégration se fait donc par simple copier-coller.
class GPersonnel : public QMainWindow
{
    Q_OBJECT

public:
    GPersonnel(QWidget *parent = 0);
    ~GPersonnel();

private slots:
    void on_btnQuitterPersonnel_clicked();

    // ----- Module Personnel (slots créés automatiquement par le nom on_<objet>_<signal>) -----
    void on_btnAjouterPersonnel_clicked();
    void on_btnModifierPersonnel_clicked();
    void on_btnSupprimerPersonnel_clicked();
    void on_btnRechercherPersonnel_clicked();
    void on_lineEditRecherchePersonnel_textChanged(const QString &texte);
    void on_btnTrierPersonnel_clicked();
    void on_btnExportPersonnel_clicked();
    void on_btnStatsPersonnel_clicked();
    void on_tableWidgetPersonnel_cellDoubleClicked(int ligne, int colonne);
    void on_spinSeuil_valueChanged(int valeur);
    void on_btnDetailsSurcharge_clicked();

private:
    Ui::GPersonnel *ui;

    // ----- Module Personnel -----
    void setupPersonnel();
    void chargerPersonnelExemple();
    void refreshPersonnelTable();
    void refreshSurcharge();
    void filtrerPersonnel();
    int personnelSelectionne();
    QString prochainIdPersonnel();
    bool doitEchangerPersonnel(Personnel a, Personnel b, int critere);
    StatCritere statPersonnelParGrade();
    StatCritere statPersonnelParDispo();
    QList<Personnel> m_personnel;

    // ----- Outils communs -----
    QTableWidgetItem *creerItem(const QString &texte, const QColor &couleur, bool gras);
    void setLabelClass(QLabel *label, const QString &classe);
    void exporterTableauPdf(QTableWidget *table, const QString &titre, const QString &nomFichier);
};

#endif // GPERSONNEL_H
