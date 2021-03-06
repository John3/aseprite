/* Aseprite
 * Copyright (C) 2001-2013  David Capello
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef APP_JOB_H_INCLUDED
#define APP_JOB_H_INCLUDED
#pragma once

#include "base/unique_ptr.h"
#include "ui/alert.h"
#include "ui/timer.h"

namespace base {
  class thread;
  class mutex;
}

namespace app {
  class Progress;

  class Job {
  public:
    Job(const char* job_name);
    virtual ~Job();

    // Starts the job calling onJob() event in another thread and
    // monitoring the progress with onMonitorTick() event.
    void startJob();

    // The onJob() can use this function to report progress of the
    // background job being done. 1.0 is completed.
    void jobProgress(double f);

    // Returns true if the job was canceled by the user (in case he
    // pressed a "Cancel" button in the GUI). The onJob() thread should
    // check this variable periodically to stop working.
    bool isCanceled();

  protected:

    // This member function is called from another dedicated thread
    // outside the GUI one, so you can do some image processing here.
    // Remember that you cannot use any GUI element in this handler.
    virtual void onJob() = 0;

    // Called each 1000 msecs by the GUI queue processing.
    // It is executed from the main GUI thread.
    virtual void onMonitoringTick();

  private:
    void done();

    static void thread_proc(Job* self);
    static void monitor_proc(void* data);
    static void monitor_free(void* data);

    base::thread* m_thread;
    base::UniquePtr<ui::Timer> m_timer;
    Progress* m_progress;
    base::mutex* m_mutex;
    ui::AlertPtr m_alert_window;
    double m_last_progress;
    bool m_done_flag;
    bool m_canceled_flag;

    // these methods are privated and not defined
    Job();
    Job(const Job&);
    Job& operator==(const Job&);

  };

} // namespace app

#endif
