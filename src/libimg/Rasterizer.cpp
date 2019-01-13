/*
 *   AviTab - Aviator's Virtual Tablet
 *   Copyright (C) 2018 Folke Will <folko@solhost.org>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Affero General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Affero General Public License for more details.
 *
 *   You should have received a copy of the GNU Affero General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <stdexcept>
#include <chrono>
#include <thread>
#include <cmath>
#include "Rasterizer.h"
#include "src/Logger.h"

namespace img {

Rasterizer::Rasterizer() {
    initFitz();
}

void Rasterizer::initFitz() {
    logger::verbose("Init fitz in thread %d", std::this_thread::get_id());
    ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    if (!ctx) {
        throw std::runtime_error("Couldn't initialize fitz");
    }

    fz_try(ctx) {
        fz_register_document_handlers(ctx);
    } fz_catch(ctx) {
        fz_drop_context(ctx);
        throw std::runtime_error("Cannot register document handlers: " + std::string(fz_caught_message(ctx)));
    }
}

int Rasterizer::getTileSize() {
    return tileSize;
}

void Rasterizer::loadDocument(const std::string& utf8Path) {
    freeCurrentDocument();

    logger::info("Loading '%s in thread %d'", utf8Path.c_str(), std::this_thread::get_id());

    fz_try(ctx) {
        doc = fz_open_document(ctx, utf8Path.c_str());
    } fz_catch(ctx) {
        throw std::runtime_error("Cannot open document: " + std::string(fz_caught_message(ctx)));
    }

    fz_try(ctx) {
        totalPages = fz_count_pages(ctx, doc);
        logger::verbose("Document has %d pages", totalPages);
    } fz_catch(ctx) {
        fz_drop_document(ctx, doc);
        throw std::runtime_error("Cannot count pages: " + std::string(fz_caught_message(ctx)));
    }

    // get initial page size
    fz_try(ctx) {
        fz_page *page = fz_load_page(ctx, doc, 0);
        fz_rect rect = fz_bound_page(ctx, page);
        currentPageWidth = rect.x1 - rect.x0;
        currentPageHeight = rect.y1 - rect.y0;
        fz_drop_page(ctx, page);
    } fz_catch(ctx) {
        fz_drop_document(ctx, doc);
        throw std::runtime_error("Cannot get first page size: " + std::string(fz_caught_message(ctx)));
    }
    logger::info("Document loaded, %dx%d", currentPageWidth, currentPageHeight);
}

void Rasterizer::setPage(int pageNum) {
    requestedPageNum = pageNum;
}

void Rasterizer::loadPage() {
    if (requestedPageNum == currentPageNum && currentPageList != nullptr) {
        return;
    }

    freeCurrentPage();

    logger::verbose("Loading page %d in thread %d", (int) requestedPageNum, std::this_thread::get_id());

    fz_try(ctx) {
        currentPageList = fz_new_display_list_from_page_number(ctx, doc, requestedPageNum);
        currentPageNum = requestedPageNum;
    } fz_catch(ctx) {
        throw std::runtime_error("Cannot parse page: " + std::string(fz_caught_message(ctx)));
    }

    fz_rect rect = fz_bound_display_list(ctx, currentPageList);
    currentPageWidth = rect.x1 - rect.x0;
    currentPageHeight = rect.y1 - rect.y0;

    logger::verbose("Page %d rasterized to %dx%d pixels", currentPageNum, currentPageWidth, currentPageHeight);
}

int Rasterizer::getPageWidth(int zoom) {
    loadPage();
    return currentPageWidth * zoomToScale(zoom);
}

int Rasterizer::getPageHeight(int zoom) {
    loadPage();
    return currentPageHeight * zoomToScale(zoom);
}

int Rasterizer::getPageCount() const {
    return totalPages;
}

int Rasterizer::getPageNum() const {
    return requestedPageNum;
}

std::unique_ptr<Image> Rasterizer::loadTile(int x, int y, int zoom) {
    loadPage();

    if (logLoadTimes) {
        logger::info("Loading tile %d, %d, %d", x, y, zoom);
    }

    auto image = std::make_unique<Image>(tileSize, tileSize, 0);

    int outStartX = tileSize * x;
    int outStartY = tileSize * y;

    int outWidth = image->getWidth();
    int outHeight = image->getHeight();

    float scale = zoomToScale(zoom);
    fz_matrix scaleMatrix = fz_scale(scale, scale);

    fz_irect clipBox;
    clipBox.x0 = outStartX;
    clipBox.x1 = outStartX + outWidth;
    clipBox.y0 = outStartY;
    clipBox.y1 = outStartY + outHeight;

    fz_pixmap *pix = nullptr;
    fz_try(ctx) {
        uint8_t *outBuf = (uint8_t *) image->getPixels();
        pix = fz_new_pixmap_with_data(ctx, fz_device_bgr(ctx), outWidth, outHeight, nullptr, 1, outWidth * 4, outBuf);
        pix->x = clipBox.x0;
        pix->y = clipBox.y0;
        pix->xres = 72; // fz_bound_page returned pixels with 72 dpi
        pix->yres = 72;
    } fz_catch(ctx) {
        throw std::runtime_error("Couldn't create pixmap: " + std::string(fz_caught_message(ctx)));
    }

    fz_device *dev = nullptr;
    fz_try(ctx) {
        auto startAt = std::chrono::steady_clock::now();

        dev = fz_new_draw_device_with_bbox(ctx, scaleMatrix, pix, &clipBox);

        // pre-fill page with white
        fz_path *path = fz_new_path(ctx);
        fz_moveto(ctx, path, 0, 0);
        fz_lineto(ctx, path, 0, currentPageHeight);
        fz_lineto(ctx, path, currentPageWidth, currentPageHeight);
        fz_lineto(ctx, path, currentPageWidth, 0);
        fz_closepath(ctx, path);
        float white = 1.0f;
        fz_fill_path(ctx, dev, path, 0, fz_identity, fz_device_gray(ctx), &white, 1.0f, nullptr);
        fz_drop_path(ctx, path);

        fz_rect pageRect;
        pageRect.x0 = 0;
        pageRect.y0 = 0;
        pageRect.x1 = currentPageWidth;
        pageRect.y1 = currentPageHeight;
        fz_run_display_list(ctx, currentPageList, dev, fz_identity, pageRect, nullptr);
        fz_close_device(ctx, dev);
        fz_drop_device(ctx, dev);

        if (logLoadTimes) {
            auto endAt = std::chrono::steady_clock::now();
            auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(endAt - startAt).count();
            logger::info("Tile loaded in %d millis", dur);
        }
    } fz_catch(ctx) {
        if (dev) {
            fz_drop_device(ctx, dev);
        }
        fz_drop_pixmap(ctx, pix);
        throw std::runtime_error("Couldn't render page: " + std::string(fz_caught_message(ctx)));
    }

    if (pix) {
        fz_drop_pixmap(ctx, pix);
    }

    return image;
}

float Rasterizer::zoomToScale(int zoom) const {
    return std::pow(M_SQRT2, zoom);
}

void Rasterizer::freeCurrentDocument() {
    freeCurrentPage();
    if (doc) {
        fz_drop_document(ctx, doc);
        doc = nullptr;
    }
}

void Rasterizer::freeCurrentPage() {
    if (currentPageList) {
        fz_drop_display_list(ctx, currentPageList);
        currentPageList = nullptr;
    }
}

Rasterizer::~Rasterizer() {
    if (ctx) {
        freeCurrentDocument();
        fz_drop_context(ctx);
    }
}

} /* namespace img */
