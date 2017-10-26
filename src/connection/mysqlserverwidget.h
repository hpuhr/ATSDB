/*
 * This file is part of ATSDB.
 *
 * ATSDB is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ATSDB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with ATSDB.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MYSQLSERVERWIDGET_H
#define MYSQLSERVERWIDGET_H

#include <qwidget.h>

class MySQLServer;
class MySQLppConnection;
class QLineEdit;
class QComboBox;
class QPushButton;

class MySQLServerWidget : public QWidget
{
    Q_OBJECT

public slots:
    void updateHostSlot (const QString &value);
    void updateUserSlot (const QString &value);
    void updatePasswordSlot (const QString &value);
    void updatePortSlot (const QString &value);
    void connectSlot ();

    void updateDatabaseSlot (const QString &value);
    void openDatabaseSlot ();

signals:
    void serverConnectedSignal ();
    void databaseOpenedSignal ();

public:
    explicit MySQLServerWidget(MySQLppConnection &connection, MySQLServer &server, QWidget *parent = 0);
    virtual ~MySQLServerWidget ();

protected:
    MySQLppConnection &connection_;
    MySQLServer &server_;

    /// MySQL ip address edit field
    QLineEdit *host_edit_;
    /// MySQL username edit field
    QLineEdit *user_edit_;
    /// MySQL password edit field
    QLineEdit *password_edit_;
    /// MySQL port edit field
    QLineEdit *port_edit_;

    /// Open connection button
    QPushButton *connect_button_;

    /// MySQL database name edit field
    //QLineEdit *mysql_db_name_edit_;
    QComboBox *db_name_box_;

    QPushButton *open_button_;

    void updateDatabases ();
};


#endif // MYSQLSERVERWIDGET_H

