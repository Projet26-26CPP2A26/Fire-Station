#include "statsdialog.h"
#include "styleutils.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

StatCritere compterParValeur(const QString &nom, const QStringList &possibles,
                             const QStringList &reelles)
{
    StatCritere resultat;
    resultat.nom = nom;
    for (int p = 0; p < possibles.size(); p++) {
        int n = 0;
        for (int r = 0; r < reelles.size(); r++) {
            if (reelles[r] == possibles[p])
                n++;
        }
        if (n > 0) { // on n'affiche pas les valeurs qui n'existent pas
            resultat.etiquettes << possibles[p];
            resultat.valeurs << n;
        }
    }
    return resultat;
}

StatsDialog::StatsDialog(const QString &titre, const QList<StatCritere> &criteres, QWidget *parent)
    : QDialog(parent)
{
    m_criteres = criteres;
    setWindowTitle(titre);
    setMinimumWidth(520);

    QLabel *labelTitre = new QLabel(titre);
    labelTitre->setProperty("class", "cardTitle");

    // Liste déroulante des critères
    m_combo = new QComboBox;
    for (int i = 0; i < m_criteres.size(); i++)
        m_combo->addItem(m_criteres[i].nom);

    QHBoxLayout *ligneCritere = new QHBoxLayout;
    ligneCritere->addWidget(new QLabel("Statistiques selon :"));
    ligneCritere->addWidget(m_combo);
    ligneCritere->addStretch();

    // Zone où les barres sont affichées
    m_grille = new QGridLayout;
    m_grille->setVerticalSpacing(14);

    QPushButton *boutonFermer = new QPushButton("Fermer");
    boutonFermer->setProperty("class", "btnPrimary");
    connect(boutonFermer, SIGNAL(clicked()), this, SLOT(close()));
    QHBoxLayout *ligneBoutons = new QHBoxLayout;
    ligneBoutons->addStretch();
    ligneBoutons->addWidget(boutonFermer);

    QVBoxLayout *principal = new QVBoxLayout(this);
    principal->setContentsMargins(24, 20, 24, 20);
    principal->setSpacing(12);
    principal->addWidget(labelTitre);
    principal->addLayout(ligneCritere);
    principal->addLayout(m_grille);
    principal->addStretch();
    principal->addLayout(ligneBoutons);

    connect(m_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(afficherCritere(int)));
    afficherCritere(0);
    repolish(this);
}

void StatsDialog::afficherCritere(int index)
{
    if (index < 0 || index >= m_criteres.size())
        return;

    // 1) On efface les anciennes barres
    QLayoutItem *element = m_grille->takeAt(0);
    while (element != 0) {
        delete element->widget();
        delete element;
        element = m_grille->takeAt(0);
    }

    // 2) On calcule le total
    StatCritere critere = m_criteres[index];
    int total = 0;
    for (int i = 0; i < critere.valeurs.size(); i++)
        total += critere.valeurs[i];

    // 3) Une ligne par valeur : étiquette | barre | nombre (pourcentage)
    for (int i = 0; i < critere.etiquettes.size(); i++) {
        int pourcentage = critere.valeurs[i] * 100 / total;

        QProgressBar *barre = new QProgressBar;
        barre->setMaximum(total);
        barre->setValue(critere.valeurs[i]);
        barre->setTextVisible(false);

        m_grille->addWidget(new QLabel(critere.etiquettes[i]), i, 0);
        m_grille->addWidget(barre, i, 1);
        m_grille->addWidget(new QLabel(QString("%1  (%2 %)").arg(critere.valeurs[i]).arg(pourcentage)), i, 2);
    }
    m_grille->setColumnStretch(1, 1);
    m_grille->setColumnMinimumWidth(0, 110);
    m_grille->addWidget(new QLabel(QString("Total : %1").arg(total)), critere.etiquettes.size(), 0, 1, 3);
}
