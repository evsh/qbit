/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2010  Christophe Dumez
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 *
 * Contact : chris@qbittorrent.org
 */

#include "torrentcreatorthread.h"

#include "config.h"

#include <fstream>

#include <boost/bind.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/storage.hpp>
#include <libtorrent/torrent_info.hpp>

#include <QFile>

#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"

namespace
{
    // do not include files and folders whose
    // name starts with a .
    bool fileFilter(const std::string &f)
    {
        return !Utils::Fs::fileName(QString::fromStdString(f)).startsWith('.');
    }
}

namespace libt = libtorrent;
using namespace BitTorrent;

TorrentCreatorThread::TorrentCreatorThread(QObject *parent)
    : QThread(parent)
{
}

TorrentCreatorThread::~TorrentCreatorThread()
{
    requestInterruption();
    wait();
}

void TorrentCreatorThread::create(const TorrentCreatorParams &params)
{
    m_params = params;
    start();
}

void TorrentCreatorThread::sendProgressSignal(int currentPieceIdx, int totalPieces)
{
    emit updateProgress(static_cast<int>((currentPieceIdx * 100.) / totalPieces));
}

void TorrentCreatorThread::run()
{
    const QString creatorStr("qBittorrent " QBT_VERSION);

    emit updateProgress(0);

    try {
        libt::file_storage fs;
        // Adding files to the torrent
        libt::add_files(fs, Utils::Fs::toNativePath(m_params.inputPath).toStdString(), fileFilter);

        if (isInterruptionRequested()) return;

        libt::create_torrent newTorrent(fs, m_params.pieceSize);

        // Add url seeds
        foreach (QString seed, m_params.urlSeeds) {
            seed = seed.trimmed();
            if (!seed.isEmpty())
                newTorrent.add_url_seed(seed.toStdString());
        }

        int tier = 0;
        foreach (const QString &tracker, m_params.trackers) {
            if (tracker.isEmpty())
                ++tier;
            else
                newTorrent.add_tracker(tracker.trimmed().toStdString(), tier);
        }

        if (isInterruptionRequested()) return;

        // calculate the hash for all pieces
        const QString parentPath = Utils::Fs::branchPath(m_params.inputPath) + "/";
        libt::set_piece_hashes(newTorrent, Utils::Fs::toNativePath(parentPath).toStdString()
            , [this, &newTorrent](const int n) { sendProgressSignal(n, newTorrent.num_pieces()); });
        // Set qBittorrent as creator and add user comment to
        // torrent_info structure
        newTorrent.set_creator(creatorStr.toUtf8().constData());
        newTorrent.set_comment(m_params.comment.toUtf8().constData());
        // Is private ?
        newTorrent.set_priv(m_params.isPrivate);

        if (isInterruptionRequested()) return;

        libt::entry entry = newTorrent.generate();

        // add source field
        if (!m_params.source.isEmpty())
            entry["info"]["source"] = m_params.source.toStdString();

        if (isInterruptionRequested()) return;

        // create the torrent
        std::ofstream outfile(
#ifdef _MSC_VER
            Utils::Fs::toNativePath(m_params.savePath).toStdWString().c_str()
#else
            Utils::Fs::toNativePath(m_params.savePath).toUtf8().constData()
#endif
            , (std::ios_base::out | std::ios_base::binary | std::ios_base::trunc));
        if (outfile.fail())
            throw std::runtime_error(tr("create new torrent file failed").toStdString());

        if (isInterruptionRequested()) return;

        libt::bencode(std::ostream_iterator<char>(outfile), entry);
        outfile.close();

        emit updateProgress(100);
        emit creationSuccess(m_params.savePath, parentPath);
    }
    catch (const std::exception &e) {
        emit creationFailure(e.what());
    }
}

int TorrentCreatorThread::calculateTotalPieces(const QString &inputPath, const int pieceSize)
{
    if (inputPath.isEmpty())
        return 0;

    libt::file_storage fs;
    libt::add_files(fs, Utils::Fs::toNativePath(inputPath).toStdString(), fileFilter);
    return libt::create_torrent(fs, pieceSize).num_pieces();
}
